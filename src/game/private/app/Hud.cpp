#include "Hud.h"

#include <cstdio>
#include <filesystem>

#include "Art.h"        // Resource icons (optional, may be fallback)
#include "Building.h"   // BuildingTypeName + selected-building summary
#include "Hotkeys.h"    // live key bindings for the hint overlay
#include "raygui.h"   // panels/labels
#include "SaveGame.h" // Slot paths
#include "Selection.h" // SelectedUnit
#include "UnitFactory.h" // CostOf for the build menu
#include "UnitStats.h" // max-health lookup for the summary

const char *UnitTypeName(UnitType type)
{
    switch (type)
    {
    case UnitType::Infantry:
        return "Infantry";
    case UnitType::AntiArmorInfantry:
        return "Anti-Armor";
    case UnitType::Engineer:
        return "Engineer";
    case UnitType::IFV:
        return "IFV";
    case UnitType::Artillery:
        return "Artillery";
    case UnitType::LightTank:
        return "Light Tank";
    case UnitType::HeavyTank:
        return "Heavy Tank";
    case UnitType::PrototypeInfantry:
        return "Prototype Infantry";
    }
    return "Unknown";
}

const char *BuildingTypeName(BuildingType type)
{
    switch (type)
    {
    case BuildingType::Base:
        return "Base";
    case BuildingType::ResourceDepot:
        return "Depot";
    case BuildingType::Factory:
        return "Factory";
    }
    return "Unknown";
}

std::string FormatResources(const ResourceSystem &resources)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Iron: %ld  Oil: %ld", resources.iron, resources.oil);
    return buf;
}

std::string SelectionSummary(const Unit &unit)
{
    const float maxHealth = BaseStats(unit.type).health;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s  HP %.0f/%.0f  Team %d", UnitTypeName(unit.type), unit.health,
                  maxHealth, unit.teamID);
    return buf;
}

float UnitHealthFraction(const Unit &unit)
{
    const float maxHealth = BaseStats(unit.type).health;
    if (maxHealth <= 0.0f)
    {
        return 0.0f;
    }
    const float fraction = unit.health / maxHealth;
    if (fraction <= 0.0f)
    {
        return 0.0f;
    }
    return fraction >= 1.0f ? 1.0f : fraction;
}

namespace
{
// Live key label for a Tier-1 action, mirroring Game::DrawHotkeyRemap:
// effective key name with a "Shift+" prefix when the def is chorded.
std::string HintKey(const HotkeyMap &hotkeys, const char *action, bool applyChord = true)
{
    const HotkeyDef *def = HotkeyDefFor(action);
    const int key = hotkeys.KeyFor(action != nullptr ? action : "");
    std::string name = key <= 0 ? "-" : HotkeyDisplayName(key);
    if (applyChord && def != nullptr && def->chord)
    {
        name = "Shift+" + name;
    }
    return name;
}
} // namespace

// Live hotkey hint overlay: Tier-1 lines resolve through HotkeyMap so a
// rebind shows up immediately; Tier-2/mouse lines have no live source and
// stay hardcoded literals. Line order matches the pre-live list.
std::vector<std::string> ShortcutHintLines(const HotkeyMap &hotkeys)
{
    std::vector<std::string> lines = {
        "WASD Camera",
        "Left Select",
        "Right Order",
    };
    lines.push_back(HintKey(hotkeys, "Back") + " Deselect");
    lines.push_back(HintKey(hotkeys, "Halt") + " Halt");
    lines.push_back(HintKey(hotkeys, "TogglePause") + " Pause");
    lines.push_back(HintKey(hotkeys, "ToggleHints") + " Hints");
    lines.push_back(HintKey(hotkeys, "Quicksave") + " Save");
    lines.push_back(HintKey(hotkeys, "Quickload") + " Load");
    lines.push_back(HintKey(hotkeys, "AttackMove") + " AttackMove");
    lines.push_back(HintKey(hotkeys, "StanceHold") + " Hold");
    lines.push_back(HintKey(hotkeys, "StanceGuard") + " Guard");
    lines.push_back(HintKey(hotkeys, "Patrol") + " Patrol");
    lines.push_back(HintKey(hotkeys, "Rally") + " Rally");
    lines.push_back(HintKey(hotkeys, "SelectType") + " SelectType");
    lines.push_back(HintKey(hotkeys, "SelectFactories") + " Factories");
    lines.push_back(HintKey(hotkeys, "SlowestSpeed") + " SlowMove");
    lines.push_back(HintKey(hotkeys, "AreaBuild") + " Place");
    lines.push_back("Alt+RDrag Line");
    lines.push_back(HintKey(hotkeys, "AttackGround") + " AttackGnd");
    lines.push_back(HintKey(hotkeys, "AutoRetreat") + " AutoRetreat");
    lines.push_back(HintKey(hotkeys, "JumpPing") + " JumpPing");
    lines.push_back("Shift Queues");
    lines.push_back("Wheel Zoom");
    // Slot ranges combine three actions; join the effective keys so a
    // rebind of any one slot stays truthful (defaults: F6/F7/F8).
    lines.push_back(HintKey(hotkeys, "SaveSlot1") + "/" + HintKey(hotkeys, "SaveSlot2") + "/" +
                    HintKey(hotkeys, "SaveSlot3") + " SaveSlot");
    lines.push_back("Shift+" + HintKey(hotkeys, "LoadSlot1", false) + "/" +
                    HintKey(hotkeys, "LoadSlot2", false) + "/" +
                    HintKey(hotkeys, "LoadSlot3", false) + " Load");
    lines.push_back("Ctrl+1-0 Group");
    lines.push_back("Shift+1-0 AddGrp");
    lines.push_back("1-0 Recall");
    lines.push_back("C+S+1-0 AutoAdd");
    return lines;
}

bool UpdateHoverTooltip(HoverTooltipState &state, Entity hovered, float dt, float delay)
{
    if (hovered != state.hovered)
    {
        state.hovered = hovered;
        state.time = 0.0f;
        return false;
    }
    state.time += dt;
    return state.hovered != kInvalidEntity && state.time >= delay;
}

namespace
{

const char *StanceName(Stance stance)
{
    switch (stance)
    {
    case Stance::Hold:
        return "Hold";
    case Stance::Guard:
        return "Guard";
    case Stance::Patrol:
        return "Patrol";
    }
    return "Unknown";
}

const char *UnitStateName(UnitState state)
{
    switch (state)
    {
    case UnitState::Idle:
        return "Idle";
    case UnitState::Moving:
        return "Moving";
    case UnitState::Attacking:
        return "Attacking";
    case UnitState::Count:
        return "Unknown";
    default:
        return "Unknown";
    }
}

} // namespace

std::vector<std::string> UnitTooltipLines(const Unit &unit)
{
    char second[64];
    std::snprintf(second, sizeof(second), "%s, %s", StanceName(unit.stance),
                  UnitStateName(unit.state));
    return { SelectionSummary(unit), second };
}

void DrawResourcePanel(const ResourceSystem &resources, const Art *art)
{
    GuiPanel({ 8.0f, 8.0f, 220.0f, 56.0f }, "Stockpile");
    if (art != nullptr && !art->UseRectangles())
    {
        art->DrawIcon(ResourceKind::Iron, { 20.0f, 34.0f });
        art->DrawIcon(ResourceKind::Oil, { 118.0f, 34.0f });
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%ld", resources.iron);
        GuiLabel({ 40.0f, 32.0f, 76.0f, 20.0f }, buf);
        std::snprintf(buf, sizeof(buf), "%ld", resources.oil);
        GuiLabel({ 138.0f, 32.0f, 76.0f, 20.0f }, buf);
        return;
    }
    GuiLabel({ 20.0f, 32.0f, 200.0f, 20.0f }, FormatResources(resources).c_str());
}

void DrawSelectionPanel(Registry &registry, const Art *art)
{
    const Entity selected = SelectedUnit(registry);
    const Unit *unit = registry.Get<Unit>(selected);
    // Bottom-left anchored (follows window height; identical to the old
    // fixed y on a 450px window).
    const float y = static_cast<float>(GetScreenHeight()) - 64.0f;
    GuiPanel({ 8.0f, y, 300.0f, 56.0f }, "Selection");
    const char *text = "No selection";
    std::string summary;
    float textX = 20.0f;
    if (unit != nullptr)
    {
        if (SelectedUnitCount(registry) > 1)
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%d units selected", SelectedUnitCount(registry));
            summary = buf;
            text = summary.c_str();
        }
        else
        {
            summary = SelectionSummary(*unit);
            text = summary.c_str();
            // Single-selection portrait: front-facing idle frame at 1.25x
            // (20x40 inside the 56px panel). Gated on atlas art existing —
            // types without frames keep today's text-only look, and new
            // atlas types light up with zero further code changes.
            if (art != nullptr)
            {
                const std::string sprite =
                    art->UnitSprite(unit->type, false, selected, 0.0f,
                                    static_cast<int>(Facing::Bottom));
                if (!sprite.empty())
                {
                    art->DrawAtlasFrame(sprite, { 4.0f, y - 4.0f },
                                        art->TeamTint(unit->teamID), 1.25f);
                    textX = 56.0f;
                }
            }
        }
    }
    else
    {
        // QoL: building selection summary (no unit selected).
        int count = 0;
        BuildingType firstType = BuildingType::Base;
        registry.Each<Building>([&](Entity, const Building &building) {
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
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%d %s selected", count,
                          BuildingTypeName(firstType));
            summary = buf;
            text = summary.c_str();
        }
    }
    GuiLabel({ textX, y + 24.0f, 308.0f - textX - 8.0f, 20.0f }, text);
}

void DrawRepairPanel(bool *enabled, float *capFraction)
{
    const float y = static_cast<float>(GetScreenHeight()) - 64.0f;
    GuiPanel({ 478.0f, y, 150.0f, 56.0f }, "Repair");
    GuiSetTooltip("Repair damaged vehicles/buildings automatically with idle Engineers");
    GuiCheckBox({ 488.0f, y + 6.0f, 16.0f, 16.0f }, "Auto", enabled);
    GuiSetTooltip("Max share of income auto-repair may spend");
    GuiSlider({ 488.0f, y + 28.0f, 130.0f, 16.0f }, "Cap", "", capFraction, 0.0f, 1.0f);
}

void DrawIdleButtons(Registry &registry, int teamID)
{
    GuiPanel({ 318.0f, static_cast<float>(GetScreenHeight()) - 64.0f, 150.0f, 56.0f },
             "Idle");
    const float y = static_cast<float>(GetScreenHeight()) - 64.0f;
    const int workers = CountIdle(registry, teamID, true);
    const int army = CountIdle(registry, teamID, false);
    char label[32];
    std::snprintf(label, sizeof(label), "Workers (%d)", workers);
    if (workers == 0)
    {
        GuiDisable();
    }
    GuiSetTooltip("Select every idle Engineer (replaces selection)");
    if (GuiButton({ 328.0f, y + 6.0f, 130.0f, 20.0f }, label))
    {
        SelectIdle(registry, teamID, true);
    }
    if (workers == 0)
    {
        GuiEnable();
    }
    std::snprintf(label, sizeof(label), "Army (%d)", army);
    if (army == 0)
    {
        GuiDisable();
    }
    GuiSetTooltip("Select every idle combat unit (replaces selection)");
    if (GuiButton({ 328.0f, y + 30.0f, 130.0f, 20.0f }, label))
    {
        SelectIdle(registry, teamID, false);
    }
    if (army == 0)
    {
        GuiEnable();
    }
}

// QoL control-group strip: 10 numbered boxes under the stockpile panel.
// Filled green when the group has members (sky-blue when fully selected),
// dimmed when empty; gold border marks the auto-add group for production.
void DrawControlGroupStrip(Registry &registry, int teamID, int autoAddGroupBit, const Art *art)
{
    const float boxW = 26.0f, boxH = 20.0f, gap = 2.0f;
    for (int bit = 0; bit < 10; ++bit)
    {
        const unsigned int mask = 1u << static_cast<unsigned int>(bit);
        int count = 0;
        bool allSelected = true;
        registry.Each<Unit>([&](Entity, const Unit &unit) {
            if (unit.teamID != teamID || (unit.controlGroups & mask) == 0)
            {
                return;
            }
            ++count;
            if (!unit.isSelected)
            {
                allSelected = false;
            }
        });
        const float x = 8.0f + static_cast<float>(bit) * (boxW + gap);
        const Color fill =
            count == 0 ? Fade(GRAY, 0.3f) : (allSelected ? SKYBLUE : DARKGREEN);
        DrawRectangle(static_cast<int>(x), 70, static_cast<int>(boxW), static_cast<int>(boxH),
                      fill);
        if (bit == autoAddGroupBit)
        {
            DrawRectangleLinesEx({ x, 70.0f, boxW, boxH }, 2.0f, GOLD);
        }
        Art::DrawUiText(art, TextFormat("%d:%d", (bit + 1) % 10, count), static_cast<int>(x) + 2,
                        74, 10, BLACK);
    }
}

std::vector<UnitType> ProductionMenuOrder()
{
    return { UnitType::Infantry,          UnitType::AntiArmorInfantry, UnitType::Engineer,
             UnitType::IFV,               UnitType::Artillery,         UnitType::LightTank,
             UnitType::HeavyTank };
}

int DrawProductionPanel(ResourceSystem &resources, ProductionQueue &queue, bool hasFactory)
{
    const float pw = 174.0f;
    const float ph = 188.0f;
    const float px = static_cast<float>(GetScreenWidth()) - pw - 10.0f;
    const float py = static_cast<float>(GetScreenHeight()) - ph - 10.0f;
    GuiPanel({ px, py, pw, ph }, "Factory");
    if (!hasFactory)
    {
        GuiLabel({ px + 12.0f, py + 22.0f, 150.0f, 20.0f }, "Need Factory");
        return kNoProductionClick;
    }
    int clicked = kNoProductionClick;
    const std::vector<UnitType> order = ProductionMenuOrder();
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        const UnitCost cost = CostOf(order[i]);
        char label[48];
        std::snprintf(label, sizeof(label), "%s %ld/%ld", UnitTypeName(order[i]), cost.iron,
                      cost.oil);
        const float y = py + 22.0f + static_cast<float>(i) * 18.0f;
        const bool affordable = resources.iron >= cost.iron && resources.oil >= cost.oil;
        if (!affordable)
        {
            GuiDisable();
        }
        GuiSetTooltip("Queue one unit, paid upfront, spawns at the rally point");
        if (GuiButton({ px + 12.0f, y, 116.0f, 16.0f }, label))
        {
            clicked = static_cast<int>(i);
        }
        if (!affordable)
        {
            GuiEnable();
        }
        // QoL repeat toggle: arms the next build of this type to rebuild
        // forever until cancelled. R = one-shot, R* = repeating.
        const char *repLabel = queue.RepeatArmed(order[i]) ? "R*" : "R";
        GuiSetTooltip("Repeat this entry indefinitely until cancelled");
        if (GuiButton({ px + 132.0f, y, 30.0f, 16.0f }, repLabel))
        {
            queue.SetRepeatArmed(order[i], !queue.RepeatArmed(order[i]));
        }
    }
    char queueLine[48];
    std::snprintf(queueLine, sizeof(queueLine), "Queue: %d", static_cast<int>(queue.Size()));
    GuiLabel({ px + 12.0f, py + ph - 22.0f, 80.0f, 16.0f }, queueLine);
    GuiSetTooltip("Cancel the unit currently building (refunds its cost)");
    if (GuiButton({ px + 94.0f, py + ph - 22.0f, 68.0f, 16.0f }, "Cancel"))
    {
        queue.CancelTop(resources);
    }
    if (clicked != kNoProductionClick)
    {
        queue.Enqueue(resources, order[static_cast<std::size_t>(clicked)],
                      queue.RepeatArmed(order[static_cast<std::size_t>(clicked)]));
    }
    return clicked;
}

void DrawSaveSlots()
{
    char line[96];
    char marks[3][8];
    for (int i = 0; i < 3; ++i)
    {
        std::error_code ec;
        const bool filled =
            std::filesystem::exists(SaveSlotPath(i + 1), ec) && !ec;
        std::snprintf(marks[i], sizeof(marks[i]), "%d%s", i + 1, filled ? "+" : "-");
    }
    std::snprintf(line, sizeof(line), "%s %s %s  (F6-8 save, S+F6-8 load)", marks[0], marks[1],
                  marks[2]);
    // Size to the rendered text: raygui draws the label with its own
    // metric (GetLineWidth: TEXT_SPACING per glyph), NOT raylib's
    // MeasureText (fontSize/10 per glyph) — the two agree only at
    // TEXT_SIZE=10 and diverge at any other UI scale, which clipped this
    // panel. GuiGetTextWidth is the exact width GuiDrawText positions
    // from, so sizing from it matches at every scale by construction.
    const int textW = GuiGetTextWidth(line);
    const float panelW = static_cast<float>(textW + 32);
    GuiPanel({ 300.0f, 8.0f, panelW, 40.0f }, "Slots");
    GuiLabel({ 312.0f, 26.0f, static_cast<float>(textW + 8), 16.0f }, line);
}
