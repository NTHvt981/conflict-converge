#include "RmlUiHud.h"

#include <cstdio>
#include <filesystem>

#include "raylib.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "Building.h"
#include "Hud.h"
#include "RmlUiHost.h"
#include "SaveGame.h"
#include "Selection.h"
#include "UnitFactory.h"

namespace {

// Mouse-only pump (see RmlUiHud docs): wheel/keys/text stay with the game
// until Phase 4 owns focus routing.
void PumpMouse(Rml::Context *context)
{
    const int modifiers = 0;
    const Vector2 mouse = GetMousePosition();
    context->ProcessMouseMove(static_cast<int>(mouse.x), static_cast<int>(mouse.y), modifiers);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        context->ProcessMouseButtonDown(0, modifiers);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        context->ProcessMouseButtonUp(0, modifiers);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        context->ProcessMouseButtonDown(1, modifiers);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
    {
        context->ProcessMouseButtonUp(1, modifiers);
    }
}

const char *DifficultyName(AIDifficulty difficulty)
{
    switch (difficulty)
    {
    case AIDifficulty::Easy:
        return "Easy";
    case AIDifficulty::Hard:
        return "Hard";
    case AIDifficulty::Medium:
    default:
        return "Medium";
    }
}

} // namespace

// Player team for every HUD readout, matching the raygui panels.
constexpr int kPlayerTeam = 0;

RmlUiHud::RmlUiHud(Registry &registry, ResourceSystem &resources, ProductionQueue &queue,
                   Simulation &sim, HotkeyMap &hotkeys, PlayingInput &playingInput,
                   AICommander &ai, const AIDifficulty &difficulty, const bool &showHints,
                   bool &playerAutoRepair, float &autoRepairCap)
    : registry_(registry)
    , resources_(resources)
    , queue_(queue)
    , sim_(sim)
    , hotkeys_(hotkeys)
    , playingInput_(playingInput)
    , ai_(ai)
    , difficulty_(difficulty)
    , showHints_(showHints)
    , playerAutoRepair_(playerAutoRepair)
    , autoRepairCap_(autoRepairCap)
{
}

bool RmlUiHud::Init(RmlUiHost &host, const std::string &dataDir)
{
    if (ready_)
    {
        return true;
    }
    context_ = host.context();
    if (context_ == nullptr)
    {
        return false;
    }
    host_ = &host;
    hudDoc_ = context_->LoadDocument(dataDir + "/hud.rml");
    if (hudDoc_ == nullptr)
    {
        Shutdown();
        return false;
    }
    if (Rml::Element *el = hudDoc_->GetElementById("idle-workers"))
    {
        el->AddEventListener("click", this);
    }
    if (Rml::Element *el = hudDoc_->GetElementById("idle-army"))
    {
        el->AddEventListener("click", this);
    }
    if (Rml::Element *el = hudDoc_->GetElementById("btn-cancel"))
    {
        el->AddEventListener("click", this);
    }
    for (int i = 0; i < 7; ++i)
    {
        char id[16];
        snprintf(id, sizeof(id), "fac-%d", i);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            el->AddEventListener("click", this);
        }
        snprintf(id, sizeof(id), "repr-%d", i);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            el->AddEventListener("click", this);
        }
    }
    if (Rml::Element *el = hudDoc_->GetElementById("repair-auto"))
    {
        el->AddEventListener("change", this);
    }
    if (Rml::Element *el = hudDoc_->GetElementById("repair-cap"))
    {
        el->AddEventListener("change", this);
    }
    host_->HideStub();
    hudDoc_->Show();
    ready_ = true;
    return true;
}

void RmlUiHud::Shutdown()
{
    if (context_ != nullptr && hudDoc_ != nullptr)
    {
        context_->UnloadDocument(hudDoc_);
        context_->Update();
    }
    hudDoc_ = nullptr;
    context_ = nullptr;
    host_ = nullptr;
    ready_ = false;
    textCache_.clear();
    hintsCache_.clear();
}

bool RmlUiHud::IsPointerOverUI()
{
    if (!ready_ || context_ == nullptr)
    {
        return false;
    }
    const Rml::Element *hover = context_->GetHoverElement();
    const bool over = hover != nullptr && IsInteractiveTag(hover->GetTagName());
    // Latch while the pressing button is held so slider drags keep owning
    // the pointer even when it leaves the control box.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        if (over)
        {
            dragLatch_ = true;
        }
    }
    else
    {
        dragLatch_ = false;
    }
    return over || dragLatch_;
}

void RmlUiHud::Draw(int screenWidth, int screenHeight, float uiScale)
{
    PumpMouse(context_);
    RefreshHud();
    host_->BeginFrame(screenWidth, screenHeight, uiScale);
}

void RmlUiHud::ProcessEvent(Rml::Event &event)
{
    Rml::Element *target = event.GetTargetElement();
    if (target == nullptr)
    {
        return;
    }
    const Rml::String type = event.GetType();
    const Rml::String id = target->GetId();
    if (type == "click")
    {
        OnClick(id);
    }
    else if (type == "change")
    {
        OnChange(target, id);
    }
}

void RmlUiHud::RefreshHud()
{
    SetTextCached("res-line", FormatResources(resources_));
    SetTextCached("sel-line", SelectionLine());
    SetTextCached("slots-line", SlotsLine());

    const int workers = CountIdle(registry_, kPlayerTeam, true);
    const int army = CountIdle(registry_, kPlayerTeam, false);
    char label[64];
    snprintf(label, sizeof(label), "Workers (%d)", workers);
    SetTextCached("idle-workers", label);
    snprintf(label, sizeof(label), "Army (%d)", army);
    SetTextCached("idle-army", label);
    if (Rml::Element *el = hudDoc_->GetElementById("idle-workers"))
    {
        SetDisabled(el, workers == 0);
    }
    if (Rml::Element *el = hudDoc_->GetElementById("idle-army"))
    {
        SetDisabled(el, army == 0);
    }

    SetCheckIn("repair-auto", playerAutoRepair_);
    SetRangeIn("repair-cap", autoRepairCap_);
    snprintf(label, sizeof(label), "%g", static_cast<double>(autoRepairCap_));
    SetTextCached("repair-cap-val", label);

    const int autoBit = playingInput_.AutoAddGroupBit();
    for (int bit = 0; bit < 10; ++bit)
    {
        const unsigned int mask = 1u << static_cast<unsigned int>(bit);
        int count = 0;
        bool allSelected = true;
        registry_.Each<Unit>([&](Entity, const Unit &unit) {
            if (unit.teamID != kPlayerTeam || (unit.controlGroups & mask) == 0)
            {
                return;
            }
            ++count;
            if (!unit.isSelected)
            {
                allSelected = false;
            }
        });
        char id[16];
        snprintf(id, sizeof(id), "cg-%d", bit);
        snprintf(label, sizeof(label), "%d:%d", (bit + 1) % 10, count);
        SetTextCached(id, label);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            const bool nonempty = count > 0;
            el->SetClass("nonempty", nonempty);
            el->SetClass("allselected", nonempty && allSelected);
            el->SetClass("autogold", bit == autoBit);
        }
    }

    const bool showRows = sim_.HasFactory();
    if (Rml::Element *rows = hudDoc_->GetElementById("factory-rows"))
    {
        rows->SetProperty("display", showRows ? "block" : "none");
    }
    if (Rml::Element *stub = hudDoc_->GetElementById("need-factory"))
    {
        stub->SetProperty("display", showRows ? "none" : "block");
    }
    if (Rml::Element *queueRow = hudDoc_->GetElementById("fac-queue-row"))
    {
        queueRow->SetProperty("display", showRows ? "block" : "none");
    }
    const std::vector<UnitType> order = ProductionMenuOrder();
    for (std::size_t i = 0; i < order.size() && i < 7; ++i)
    {
        const UnitCost cost = CostOf(order[i]);
        char id[16];
        snprintf(id, sizeof(id), "fac-%d", static_cast<int>(i));
        snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(order[i]), cost.iron,
                 cost.oil);
        SetTextCached(id, label);
        if (Rml::Element *el = hudDoc_->GetElementById(id))
        {
            SetDisabled(el, resources_.iron < cost.iron || resources_.oil < cost.oil);
        }
        snprintf(id, sizeof(id), "repr-%d", static_cast<int>(i));
        SetTextCached(id, queue_.RepeatArmed(order[i]) ? "R*" : "R");
    }
    snprintf(label, sizeof(label), "Queue: %d", static_cast<int>(queue_.Size()));
    SetTextCached("queue-line", label);

    if (Rml::Element *producing = hudDoc_->GetElementById("hud-producing"))
    {
        producing->SetProperty("display", queue_.Empty() ? "none" : "block");
    }
    if (Rml::Element *fill = hudDoc_->GetElementById("producing-fill"))
    {
        char width[32];
        snprintf(width, sizeof(width), "%g%%",
                 static_cast<double>(queue_.HeadProgress() * 100.0f));
        fill->SetProperty(Rml::String("width"), Rml::String(width));
    }

    snprintf(label, sizeof(label), "%d FPS", GetFPS());
    SetTextCached("fps-line", label);
    snprintf(label, sizeof(label), "Enemy: %s  Waves: %d", DifficultyName(difficulty_),
             ai_.WavesLaunched());
    SetTextCached("enemy-line", label);

    if (Rml::Element *hints = hudDoc_->GetElementById("hud-hints"))
    {
        hints->SetProperty("display", showHints_ ? "block" : "none");
    }
    RefreshHints();
}

std::string RmlUiHud::SelectionLine() const
{
    const Entity selected = SelectedUnit(registry_);
    const Unit *unit = registry_.Get<Unit>(selected);
    char buf[128];
    if (unit != nullptr)
    {
        if (SelectedUnitCount(registry_) > 1)
        {
            snprintf(buf, sizeof(buf), "%d units selected", SelectedUnitCount(registry_));
            return buf;
        }
        return SelectionSummary(*unit);
    }
    int count = 0;
    BuildingType firstType = BuildingType::Base;
    registry_.Each<Building>([&](Entity, const Building &building) {
        if (building.isSelected)
        {
            if (count == 0)
            {
                firstType = building.type;
            }
            ++count;
        }
    });
    if (count > 0)
    {
        snprintf(buf, sizeof(buf), "%d %s selected", count, BuildingTypeName(firstType));
        return buf;
    }
    return "No selection";
}

std::string RmlUiHud::SlotsLine() const
{
    char marks[3][8];
    for (int i = 0; i < 3; ++i)
    {
        std::error_code ec;
        const bool filled = std::filesystem::exists(SaveSlotPath(i + 1), ec) && !ec;
        snprintf(marks[i], sizeof(marks[i]), "%d%s", i + 1, filled ? "+" : "-");
    }
    char line[96];
    snprintf(line, sizeof(line), "%s %s %s  (F6-8 save, S+F6-8 load)", marks[0], marks[1],
             marks[2]);
    return line;
}

void RmlUiHud::RefreshHints()
{
    const std::vector<std::string> hints = ShortcutHintLines(hotkeys_);
    std::string joined;
    for (const std::string &line : hints)
    {
        joined += line;
        joined += '\n';
    }
    if (joined == hintsCache_)
    {
        return;
    }
    hintsCache_ = joined;
    Rml::Element *list = hudDoc_->GetElementById("hud-hints");
    if (list == nullptr)
    {
        return;
    }
    while (Rml::Element *child = list->GetFirstChild())
    {
        list->RemoveChild(child);
    }
    for (const std::string &line : hints)
    {
        Rml::ElementPtr item(hudDoc_->CreateElement("p"));
        item->SetAttribute("class", Rml::String("hint-line"));
        item->SetInnerRML(line.c_str());
        list->AppendChild(std::move(item));
    }
}

void RmlUiHud::OnClick(const Rml::String &id)
{
    const std::vector<UnitType> order = ProductionMenuOrder();
    if (id == "idle-workers")
    {
        if (CountIdle(registry_, kPlayerTeam, true) > 0)
        {
            SelectIdle(registry_, kPlayerTeam, true);
        }
    }
    else if (id == "idle-army")
    {
        if (CountIdle(registry_, kPlayerTeam, false) > 0)
        {
            SelectIdle(registry_, kPlayerTeam, false);
        }
    }
    else if (id == "btn-cancel")
    {
        queue_.CancelTop(resources_);
    }
    else if (id.compare(0, 4, "fac-") == 0)
    {
        // Enqueue re-validates affordability (TrySpend); the disabled class
        // is visual-only, matching the raygui panel.
        const int row = std::atoi(id.c_str() + 4);
        if (row >= 0 && row < static_cast<int>(order.size()))
        {
            queue_.Enqueue(resources_, order[static_cast<std::size_t>(row)],
                           queue_.RepeatArmed(order[static_cast<std::size_t>(row)]));
        }
    }
    else if (id.compare(0, 5, "repr-") == 0)
    {
        const int row = std::atoi(id.c_str() + 5);
        if (row >= 0 && row < static_cast<int>(order.size()))
        {
            const UnitType type = order[static_cast<std::size_t>(row)];
            queue_.SetRepeatArmed(type, !queue_.RepeatArmed(type));
        }
    }
}

void RmlUiHud::OnChange(Rml::Element *target, const Rml::String &id)
{
    // Same no-refresh rule as the menus: programmatic SetValue/SetAttribute
    // re-dispatches Change into this listener.
    if (id == "repair-auto")
    {
        playerAutoRepair_ = target->HasAttribute("checked");
    }
    else if (id == "repair-cap")
    {
        autoRepairCap_ = Clamp(ReadRange(target), 0.0f, 1.0f);
    }
}

void RmlUiHud::SetDisabled(Rml::Element *el, bool disabled)
{
    el->SetClass("disabled", disabled);
}

float RmlUiHud::Clamp(float value, float lo, float hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

float RmlUiHud::ReadRange(Rml::Element *el)
{
    if (auto *control = dynamic_cast<Rml::ElementFormControl *>(el))
    {
        try
        {
            return std::stof(control->GetValue().c_str());
        }
        catch (const std::exception &)
        {
        }
    }
    return 0.0f;
}

bool RmlUiHud::IsInteractiveTag(const Rml::String &tag)
{
    return tag == "button" || tag == "input" || tag == "select" || tag == "textarea";
}

void RmlUiHud::SetTextCached(const char *id, const std::string &text)
{
    auto it = textCache_.find(id);
    if (it != textCache_.end() && it->second == text)
    {
        return;
    }
    textCache_[id] = text;
    if (Rml::Element *el = hudDoc_->GetElementById(id))
    {
        el->SetInnerRML(text.c_str());
    }
}

void RmlUiHud::SetRangeIn(const char *id, float value)
{
    if (Rml::Element *el = hudDoc_->GetElementById(id))
    {
        if (auto *control = dynamic_cast<Rml::ElementFormControl *>(el))
        {
            char text[32];
            snprintf(text, sizeof(text), "%g", static_cast<double>(value));
            control->SetValue(text);
        }
    }
}

void RmlUiHud::SetCheckIn(const char *id, bool checked)
{
    if (Rml::Element *el = hudDoc_->GetElementById(id))
    {
        if (checked)
        {
            el->SetAttribute("checked", Rml::String("checked"));
        }
        else
        {
            el->RemoveAttribute("checked");
        }
    }
}
