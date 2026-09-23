#include "unit_editor.h"

#include <cstdio>

#include "raylib.h"
#include "raygui.h"

#include "Art.h"

namespace
{

const char *kTypeList = "RifleInfantry;AntiArmorInfantry;Engineer;Medic;IFV;Artillery;LightTank;HeavyTank;PrototypeInfantry";
const char *kArmorList = "STEEL;RUBBER;COMPOSITE";
const char *kDamageList = "KINETIC;EXPLOSIVE;ENERGY";

// Canonical save path (snake_case stems live in UnitConfigFilename — the
// single copy, so the editor can never drift from the loader).
std::string ConfigPath(UnitType type)
{
    return std::string("data/configs/") + UnitConfigFilename(type) + ".json";
}

int ArmorIndex(ArmorType armor)
{
    return armor == ArmorType::RUBBER ? 1 : (armor == ArmorType::COMPOSITE ? 2 : 0);
}

int DamageIndex(DamageType damage)
{
    return damage == DamageType::EXPLOSIVE ? 1 : (damage == DamageType::ENERGY ? 2 : 0);
}

void SyncBuffers(EditorState &editor)
{
    const UnitConfig &config = editor.configs[editor.selected];
    std::snprintf(editor.spritePrefix, sizeof(editor.spritePrefix), "%s",
                  config.spritePrefix.c_str());
    std::snprintf(editor.atlasIdle, sizeof(editor.atlasIdle), "%s",
                  config.atlasIdlePrefix.c_str());
    std::snprintf(editor.atlasWalk, sizeof(editor.atlasWalk), "%s",
                  config.atlasWalkPrefix.c_str());
    editor.armorActive = ArmorIndex(config.stats.armorType);
    editor.damageActive = DamageIndex(config.stats.damageType);
    editor.attackPowerBox = config.stats.attackPower;
    editor.attackRangeBox = config.stats.attackRange;
    editor.boundLBox = config.boundLeft;
    editor.boundTBox = config.boundTop;
    editor.boundRBox = config.boundLeft + config.footprintWidth;
    editor.boundBBox = config.boundTop + config.footprintHeight;
    editor.originXBox = config.originX;
    editor.originYBox = config.originY;
    editor.spriteEdit = editor.idleEdit = editor.walkEdit = false;
    editor.armorEdit = editor.damageEdit = false;
    editor.attackPowerEdit = editor.attackRangeEdit = false;
    editor.boundLEdit = editor.boundTEdit = editor.boundREdit = false;
    editor.boundBEdit = editor.originXEdit = editor.originYEdit = false;
}

void SaveOne(EditorState &editor, Art &art, int typeIndex)
{
    const UnitType type = static_cast<UnitType>(typeIndex);
    const std::string path = ConfigPath(type);
    if (SaveUnitConfig(path, editor.configs[typeIndex]))
    {
        editor.fromFile[typeIndex] = true;
        std::snprintf(editor.status, sizeof(editor.status), "Saved %s",
                      path.c_str());
        art.LoadAtlas(); // sprite-tab edits take effect immediately
    }
    else
    {
        std::snprintf(editor.status, sizeof(editor.status), "Save FAILED: %s",
                      path.c_str());
    }
}

// One labeled float-slider row; returns the row's bottom edge.
float SliderRow(const char *label, float x, float y, float w, float *value, float min,
                float max, const char *fmt)
{
    GuiLabel({ x, y, 130.0f, 20.0f }, label);
    GuiSlider({ x + 135.0f, y, w - 135.0f - 70.0f, 20.0f }, nullptr, nullptr, value,
              min, max);
    char buf[32];
    std::snprintf(buf, sizeof(buf), fmt, static_cast<double>(*value));
    GuiLabel({ x + w - 65.0f, y, 65.0f, 20.0f }, buf);
    return y + 26.0f;
}

void DrawStatsTab(EditorState &editor)
{
    UnitConfig &config = editor.configs[editor.selected];
    float y = 84.0f;
    constexpr float kX = 240.0f;
    constexpr float kW = 736.0f;
    y = SliderRow("Health", kX, y, kW, &config.stats.health, 20.0f, 1000.0f, "%.0f");
    y = SliderRow("Cooldown (s)", kX, y, kW, &config.stats.cooldownTime, 0.1f, 5.0f,
                  "%.2f");
    y = SliderRow("Speed (px/s)", kX, y, kW, &config.stats.speed, 16.0f, 256.0f, "%.0f");
    y = SliderRow("Sight (px)", kX, y, kW, &config.stats.sightRange, 0.0f, 640.0f,
                  "%.0f");

    GuiLabel({ kX, y, 130.0f, 20.0f }, "Attack power");
    if (GuiValueBox({ kX + 135.0f, y, 120.0f, 20.0f }, nullptr, &editor.attackPowerBox,
                     0, 999, editor.attackPowerEdit))
    {
        editor.attackPowerEdit = !editor.attackPowerEdit;
    }
    config.stats.attackPower = editor.attackPowerBox;
    GuiLabel({ kX + 400.0f, y, 130.0f, 20.0f }, "Attack range (px)");
    if (GuiValueBox({ kX + 535.0f, y, 120.0f, 20.0f }, nullptr, &editor.attackRangeBox,
                     0, 1024, editor.attackRangeEdit))
    {
        editor.attackRangeEdit = !editor.attackRangeEdit;
    }
    config.stats.attackRange = editor.attackRangeBox;
    y += 30.0f;

    GuiLabel({ kX, y, 130.0f, 20.0f }, "Armor");
    if (GuiDropdownBox({ kX + 135.0f, y, 160.0f, 20.0f }, kArmorList,
                       &editor.armorActive, editor.armorEdit))
    {
        editor.armorEdit = !editor.armorEdit;
    }
    if (editor.armorActive == 1)
    {
        config.stats.armorType = ArmorType::RUBBER;
    }
    else if (editor.armorActive == 2)
    {
        config.stats.armorType = ArmorType::COMPOSITE;
    }
    else
    {
        config.stats.armorType = ArmorType::STEEL;
    }
    GuiLabel({ kX + 400.0f, y, 130.0f, 20.0f }, "Damage");
    if (GuiDropdownBox({ kX + 535.0f, y, 160.0f, 20.0f }, kDamageList,
                       &editor.damageActive, editor.damageEdit))
    {
        editor.damageEdit = !editor.damageEdit;
    }
    if (editor.damageActive == 1)
    {
        config.stats.damageType = DamageType::EXPLOSIVE;
    }
    else if (editor.damageActive == 2)
    {
        config.stats.damageType = DamageType::ENERGY;
    }
    else
    {
        config.stats.damageType = DamageType::KINETIC;
    }
}

// Sprite preview for the sprite tab: the game's own rendering for this
// type at 1:1 (UnitSprite atlas lookup, flat-PNG fallback + note when
// unresolvable).
void DrawPreview(EditorState &editor, Art &art, float panelX, float panelY)
{
    const UnitType type = static_cast<UnitType>(editor.selected);
    GuiPanel({ panelX, panelY, 220.0f, 260.0f }, "Preview (1:1)");
    // 32x32 body centered in the panel.
    const Vector2 tileCorner = { panelX + 110.0f - 16.0f, panelY + 90.0f - 16.0f };
    const std::string sprite = art.UnitSprite(type, editor.animatePreview, 0,
                                              static_cast<float>(GetTime()),
                                              editor.previewFacing);
    if (!sprite.empty())
    {
        art.DrawAtlasFrame(sprite, tileCorner, WHITE, Art::BaseArtScale(type));
    }
    else
    {
        art.DrawUnit(type, 0, UnitFrame::Idle, tileCorner);
        GuiLabel({ panelX + 10.0f, panelY + 200.0f, 200.0f, 20.0f },
                 "no atlas entry:");
        GuiLabel({ panelX + 10.0f, panelY + 218.0f, 200.0f, 20.0f },
                 "flat PNG fallback");
    }
}

void DrawSpritesTab(EditorState &editor, Art &art)
{
    UnitConfig &config = editor.configs[editor.selected];
    constexpr float kX = 240.0f;
    GuiLabel({ kX, 84.0f, 150.0f, 20.0f }, "Sprite prefix");
    if (GuiTextBox({ kX + 155.0f, 84.0f, 220.0f, 20.0f }, editor.spritePrefix,
                   static_cast<int>(sizeof(editor.spritePrefix)), editor.spriteEdit))
    {
        editor.spriteEdit = !editor.spriteEdit;
    }
    config.spritePrefix = editor.spritePrefix;
    GuiLabel({ kX, 110.0f, 150.0f, 20.0f }, "Atlas idle prefix");
    if (GuiTextBox({ kX + 155.0f, 110.0f, 220.0f, 20.0f }, editor.atlasIdle,
                   static_cast<int>(sizeof(editor.atlasIdle)), editor.idleEdit))
    {
        editor.idleEdit = !editor.idleEdit;
    }
    config.atlasIdlePrefix = editor.atlasIdle;
    GuiLabel({ kX, 136.0f, 150.0f, 20.0f }, "Atlas walk prefix");
    if (GuiTextBox({ kX + 155.0f, 136.0f, 220.0f, 20.0f }, editor.atlasWalk,
                   static_cast<int>(sizeof(editor.atlasWalk)), editor.walkEdit))
    {
        editor.walkEdit = !editor.walkEdit;
    }
    config.atlasWalkPrefix = editor.atlasWalk;
    GuiLabel({ kX, 162.0f, 380.0f, 20.0f },
             "Empty atlas prefix = flat PNG fallback (valid).");
    GuiCheckBox({ kX, 188.0f, 16.0f, 16.0f }, "Animate preview",
                &editor.animatePreview);
    GuiLabel({ kX, 214.0f, 150.0f, 20.0f }, "Facing (0-7)");
    if (GuiValueBox({ kX + 155.0f, 214.0f, 100.0f, 20.0f }, nullptr, &editor.previewFacing,
                     0, 7, editor.previewFacingEdit))
    {
        editor.previewFacingEdit = !editor.previewFacingEdit;
    }

    DrawPreview(editor, art, 640.0f, 84.0f);
}

void DrawCollisionTab(EditorState &editor)
{
    UnitConfig &config = editor.configs[editor.selected];
    constexpr float kX = 240.0f;
    // Bounds L/T/R/B (tiles, R/B exclusive) + origin X/Y. Footprint
    // derives as R-L x B-T (readout below); the loader rejects
    // malformed rects back to the type baseline on reload.
    GuiLabel({ kX, 84.0f, 150.0f, 20.0f }, "Bounds L");
    if (GuiValueBox({ kX + 155.0f, 84.0f, 100.0f, 20.0f }, nullptr, &editor.boundLBox,
                     0, 8, editor.boundLEdit))
    {
        editor.boundLEdit = !editor.boundLEdit;
    }
    config.boundLeft = editor.boundLBox;
    GuiLabel({ kX + 280.0f, 84.0f, 150.0f, 20.0f }, "Bounds T");
    if (GuiValueBox({ kX + 435.0f, 84.0f, 100.0f, 20.0f }, nullptr, &editor.boundTBox,
                     0, 8, editor.boundTEdit))
    {
        editor.boundTEdit = !editor.boundTEdit;
    }
    config.boundTop = editor.boundTBox;
    GuiLabel({ kX, 110.0f, 150.0f, 20.0f }, "Bounds R");
    if (GuiValueBox({ kX + 155.0f, 110.0f, 100.0f, 20.0f }, nullptr, &editor.boundRBox,
                     0, 8, editor.boundREdit))
    {
        editor.boundREdit = !editor.boundREdit;
    }
    config.footprintWidth = editor.boundRBox - config.boundLeft;
    GuiLabel({ kX + 280.0f, 110.0f, 150.0f, 20.0f }, "Bounds B");
    if (GuiValueBox({ kX + 435.0f, 110.0f, 100.0f, 20.0f }, nullptr, &editor.boundBBox,
                     0, 8, editor.boundBEdit))
    {
        editor.boundBEdit = !editor.boundBEdit;
    }
    config.footprintHeight = editor.boundBBox - config.boundTop;
    GuiLabel({ kX, 136.0f, 150.0f, 20.0f }, "Origin X");
    if (GuiValueBox({ kX + 155.0f, 136.0f, 100.0f, 20.0f }, nullptr, &editor.originXBox,
                     0, 8, editor.originXEdit))
    {
        editor.originXEdit = !editor.originXEdit;
    }
    config.originX = editor.originXBox;
    GuiLabel({ kX + 280.0f, 136.0f, 150.0f, 20.0f }, "Origin Y");
    if (GuiValueBox({ kX + 435.0f, 136.0f, 100.0f, 20.0f }, nullptr, &editor.originYBox,
                     0, 8, editor.originYEdit))
    {
        editor.originYEdit = !editor.originYEdit;
    }
    config.originY = editor.originYBox;
    char footprint[64];
    std::snprintf(footprint, sizeof(footprint), "Footprint: %dx%d tiles (R-L x B-T)",
                  config.footprintWidth, config.footprintHeight);
    GuiLabel({ kX, 162.0f, 380.0f, 20.0f }, footprint);
    GuiLabel({ kX, 180.0f, 380.0f, 20.0f }, "Origin has no gameplay effect yet.");
}

} // namespace

void InitEditor(EditorState &editor)
{
    for (int i = 0; i < EditorState::kTypeCount; ++i)
    {
        const UnitType type = static_cast<UnitType>(i);
        UnitConfig config;
        if (LoadUnitConfig(ConfigPath(type), config))
        {
            editor.configs[i] = config;
            editor.fromFile[i] = true;
        }
        else
        {
            editor.configs[i] = DefaultUnitConfig(type);
            editor.fromFile[i] = false;
        }
    }
    editor.selected = 0;
    editor.tab = EditorState::kTabStats;
    std::snprintf(editor.status, sizeof(editor.status), "Loaded data/configs");
    SyncBuffers(editor);
}

void DrawEditorFrame(EditorState &editor, Art &art)
{
    GuiPanel({ 8.0f, 8.0f, 208.0f, 560.0f }, "Unit Types");
    int active = editor.selected;
    GuiListView({ 16.0f, 40.0f, 192.0f, 470.0f }, kTypeList, &editor.listScroll,
                &active);
    if (active != editor.selected)
    {
        editor.selected = active;
        SyncBuffers(editor);
    }
    GuiLabel({ 16.0f, 516.0f, 192.0f, 20.0f },
             editor.fromFile[editor.selected] ? "source: file" : "source: defaults");
    if (GuiButton({ 16.0f, 540.0f, 92.0f, 20.0f }, "Save"))
    {
        SaveOne(editor, art, editor.selected);
    }
    if (GuiButton({ 116.0f, 540.0f, 92.0f, 20.0f }, "Save All"))
    {
        for (int i = 0; i < EditorState::kTypeCount; ++i)
        {
            SaveOne(editor, art, i);
        }
        std::snprintf(editor.status, sizeof(editor.status), "Saved all 7 types");
    }

    if (GuiButton({ 224.0f, 8.0f, 120.0f, 28.0f }, "Stats"))
    {
        editor.tab = EditorState::kTabStats;
    }
    if (GuiButton({ 352.0f, 8.0f, 120.0f, 28.0f }, "Sprites"))
    {
        editor.tab = EditorState::kTabSprites;
    }
    if (GuiButton({ 480.0f, 8.0f, 120.0f, 28.0f }, "Collision"))
    {
        editor.tab = EditorState::kTabCollision;
    }
    GuiPanel({ 224.0f, 44.0f, 768.0f, 524.0f },
             editor.configs[editor.selected].type.c_str());

    if (editor.tab == EditorState::kTabSprites)
    {
        DrawSpritesTab(editor, art);
    }
    else if (editor.tab == EditorState::kTabCollision)
    {
        DrawCollisionTab(editor);
    }
    else
    {
        DrawStatsTab(editor);
    }

    GuiLabel({ 224.0f, 576.0f, 768.0f, 24.0f }, editor.status);
}
