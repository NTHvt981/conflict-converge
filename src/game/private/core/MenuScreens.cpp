
#include "core/MenuScreens.h"

#include "raygui.h"
#include "economy/Nodes.h"

MenuScreens::MenuScreens(MenuFlow &menu, Art &art, Audio &audio, InputManager &input,
                         HotkeyMap &hotkeys, EventDispatcher &events, MenuCallbacks callbacks)
    : menu_(menu)
    , art_(art)
    , audio_(audio)
    , input_(input)
    , hotkeys_(hotkeys)
    , events_(events)
    , callbacks_(std::move(callbacks))
{
}

void MenuScreens::OpenEditor()
{
    editorMap_.name = "Custom";
    editorMap_.author = "";
    editorMap_.width = 24;
    editorMap_.height = 18;
    editorMap_.terrain.assign(static_cast<std::size_t>(24 * 18), TerrainType::Grass);
    editorMap_.nodes.clear();
    editorMap_.playerSpawns.clear();
    editorMap_.aiSpawns.clear();
    editorBrush_ = '.';
    editorStatus_ = "";
    menu_.state = MenuState::MapEditor;
}

void MenuScreens::ApplyHotkeyOverrides()
{
    hotkeys_.ClearOverrides();
    for (const auto &override : menu_.settings.hotkeyOverrides)
    {
        hotkeys_.Rebind(override.first, override.second);
    }
}

void MenuScreens::SyncHotkeySettings()
{
    menu_.settings.hotkeyOverrides = hotkeys_.Overrides();
}

ConfirmChoice MenuScreens::Draw(int screenWidth, int screenHeight, float menuStateTime)
{
    audio_.ApplySettings(menu_.settings.masterVolume, menu_.settings.musicVolume,
                         menu_.settings.sfxVolume, menu_.settings.mute);
    audio_.UpdateMusic();

    BeginDrawing();
    ClearBackground(RAYWHITE);
    const float cx = screenWidth / 2.0f;
    if (menu_.state == MenuState::MapEditor)
    {
        constexpr float kCell = 24.0f, kOx = 20.0f, kOy = 100.0f;
        auto saveEditor = [&](const std::string &name) -> std::string {
            if (!IsValidMapSaveName(name))
            {
                return "Save refused (bad name)";
            }
            const std::string path = "data/maps/" + name + ".map";
            if (!WriteMapFile(editorMap_, path))
            {
                return "Save failed (invalid dims)";
            }
            std::string warning;
            return ValidateMapPlayable(editorMap_, &warning) ? "Saved to " + path
                                                             : "Saved with warnings: " + warning;
        };
        GuiLabel({ 20.0f, 36.0f, 600.0f, 28.0f },
                 TextFormat("Map Editor - data/maps/%s.map", editorSaveName_));
        const char kBrushes[9] = { '.', '~', 'T', '^', 'B', 'I', 'O', '1', '2' };
        const int kPaletteKeys[9] = { KEY_ONE, KEY_TWO,   KEY_THREE, KEY_FOUR, KEY_FIVE,
                                      KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE };
        for (int i = 0; i < 9; ++i)
        {
            if (IsKeyPressed(kPaletteKeys[i]))
            {
                editorBrush_ = kBrushes[i];
            }
        }
        int brushIndex = 0;
        for (int i = 0; i < 9; ++i)
        {
            if (kBrushes[i] == editorBrush_)
            {
                brushIndex = i;
            }
        }
        GuiLabel({ 660.0f, 96.0f, 200.0f, 20.0f }, "Brush (keys 1-9)");
        GuiToggleGroup({ 660.0f, 118.0f, 252.0f, 24.0f }, ".;~;T;^;B;I;O;1;2",
                       &brushIndex);
        editorBrush_ = kBrushes[brushIndex];
        const Vector2 mouse = input_.MouseScreen();
        const cc::IVec2 hover{ static_cast<int>((mouse.x - kOx) / kCell),
                               static_cast<int>((mouse.y - kOy) / kCell) };
        if (input_.LeftDown())
        {
            PaintEditorCell(editorMap_, editorBrush_, hover);
        }
        if (input_.RightPressed())
        {
            PaintEditorCell(editorMap_, '.', hover);
        }
        for (int y = 0; y < editorMap_.height; ++y)
        {
            for (int x = 0; x < editorMap_.width; ++x)
            {
                const TerrainType terrain = editorMap_
                                                .terrain[static_cast<std::size_t>(y) *
                                                             editorMap_.width +
                                                         x];
                Color fill = { 20, 60, 20, 255 };
                if (terrain == TerrainType::Water)
                {
                    fill = BLUE;
                }
                else if (terrain == TerrainType::Forest)
                {
                    fill = DARKGREEN;
                }
                else if (terrain == TerrainType::Rock)
                {
                    fill = GRAY;
                }
                else if (terrain == TerrainType::Building)
                {
                    fill = BROWN;
                }
                DrawRectangle(static_cast<int>(kOx + x * kCell),
                              static_cast<int>(kOy + y * kCell), static_cast<int>(kCell),
                              static_cast<int>(kCell), fill);
                DrawRectangleLinesEx(
                    { kOx + x * kCell, kOy + y * kCell, kCell, kCell }, 1.0f,
                    Fade(LIGHTGRAY, 0.4f));
            }
        }
        for (const MapNodeSpawn &spawn : editorMap_.nodes)
        {
            Art::DrawUiText(&art_, spawn.kind == ResourceKind::Iron ? "I" : "O",
                     static_cast<int>(kOx + spawn.tile.x * kCell) + 7,
                     static_cast<int>(kOy + spawn.tile.y * kCell) + 3, 16, WHITE);
        }
        for (const cc::IVec2 &tile : editorMap_.playerSpawns)
        {
            Art::DrawUiText(&art_, "1", static_cast<int>(kOx + tile.x * kCell) + 7,
                     static_cast<int>(kOy + tile.y * kCell) + 3, 16, WHITE);
        }
        for (const cc::IVec2 &tile : editorMap_.aiSpawns)
        {
            Art::DrawUiText(&art_, "2", static_cast<int>(kOx + tile.x * kCell) + 7,
                     static_cast<int>(kOy + tile.y * kCell) + 3, 16, WHITE);
        }
        GuiLabel({ 660.0f, 150.0f, 260.0f, 20.0f }, "Left-drag paints, right-click erases");
        GuiLabel({ 660.0f, 170.0f, 260.0f, 20.0f },
                 "Maps need '1', '2' and a connecting path");
        if (GuiButton({ 660.0f, 200.0f, 200.0f, 40.0f }, "Save"))
        {
            editorStatus_ = saveEditor(editorSaveName_);
        }
        if (GuiButton({ 660.0f, 248.0f, 200.0f, 30.0f }, "Save As..."))
        {
            editorSaveAsOpen_ = true;
            editorSaveAsBtn_ = 0;
        }
        if (GuiButton({ 660.0f, 286.0f, 200.0f, 40.0f }, "Back"))
        {
            editorSaveAsOpen_ = false;
            menu_.OpenMainMenu();
        }
        GuiLabel({ 660.0f, 334.0f, 260.0f, 20.0f }, editorStatus_.c_str());
        if (editorSaveAsOpen_)
        {
            const int pressed = GuiTextInputBox(
                { cx - 150.0f, 220.0f, 300.0f, 170.0f }, "Save As",
                "Map name (data/maps/):", editorSaveName_,
                static_cast<int>(sizeof(editorSaveName_)), "Save;Cancel", &editorSaveAsBtn_,
                nullptr);
            if (pressed != 0)
            {
                if (editorSaveAsBtn_ == 1)
                {
                    editorStatus_ = saveEditor(editorSaveName_);
                }
                editorSaveAsOpen_ = false;
                editorSaveAsBtn_ = 0;
            }
        }
    }
    else
    {
        menu_.OpenMainMenu();
    }
    if (const float fade = MenuFadeAlpha(menuStateTime); fade < 1.0f)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 1.0f - fade));
    }
    EndDrawing();
    return ConfirmChoice::None;
}
