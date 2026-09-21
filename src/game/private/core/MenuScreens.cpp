
#include "MenuScreens.h"

#include "raygui.h"
#include "Nodes.h"
#include "SaveGame.h"

#include <filesystem>

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

void MenuScreens::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
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

void MenuScreens::BeginRemap(MenuState returnTo)
{
    remapArming_ = -1;
    remapConflictAction_.clear();
    remapReturn_ = returnTo;
    Announce(EventType::MenuAction);
    menu_.state = MenuState::HotkeyRemap;
}

bool MenuScreens::CancelRemapCapture()
{
    if (menu_.state != MenuState::HotkeyRemap)
    {
        return false;
    }
    if (remapArming_ >= 0)
    {
        remapArming_ = -1;
    }
    else
    {
        remapConflictAction_.clear();
        menu_.state = remapReturn_;
    }
    return true;
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
    // The confirm modal owns input while open: lock raygui so menu controls
    // behind it cannot fire.
    const bool modal = menu_.ConfirmOpen();
    if (modal)
    {
        GuiLock();
    }
    const float cx = screenWidth / 2.0f;
    if (menu_.state == MenuState::MainMenu)
    {
        Art::DrawUiText(&art_, "CONFLICT CONVERGE", static_cast<int>(cx) - 290, 110, 52,
                        DARKGRAY);
        Art::DrawUiText(&art_, "real-time strategy demo", static_cast<int>(cx) - 140, 175, 20,
                        GRAY);
        if (GuiButton({ cx - 130.0f, 245.0f, 260.0f, 40.0f }, "Start Skirmish"))
        {
            menu_.OpenSetup(ListMaps("data/maps"));
            Announce(EventType::MenuAction);
        }
        if (GuiButton({ cx - 130.0f, 295.0f, 260.0f, 40.0f }, "Load Game"))
        {
            menu_.OpenLoad();
            Announce(EventType::MenuAction);
        }
        if (GuiButton({ cx - 130.0f, 345.0f, 260.0f, 40.0f }, "Settings"))
        {
            menu_.OpenSettings();
            Announce(EventType::MenuAction);
        }
        if (GuiButton({ cx - 130.0f, 395.0f, 260.0f, 40.0f }, "Map Editor"))
        {
            OpenEditor();
            Announce(EventType::MenuAction);
        }
        if (GuiButton({ cx - 130.0f, 445.0f, 260.0f, 40.0f }, "Quit"))
        {
            Announce(EventType::MenuAction);
            menu_.OpenQuitConfirm();
        }
    }
    else if (menu_.state == MenuState::SkirmishSetup)
    {
        Art::DrawUiText(&art_, "Skirmish setup", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
        GuiLabel({ cx - 200.0f, 80.0f, 400.0f, 20.0f }, "Map (from data/*.map)");
        std::string items;
        for (const MapEntry &entry : menu_.setup.maps)
        {
            items += entry.name + " (" + std::to_string(entry.width) + "x" +
                     std::to_string(entry.height) + ");";
        }
        if (items.empty())
        {
            items = "<no maps found>;";
        }
        int picked = menu_.setup.mapIndex;
        GuiListView({ cx - 200.0f, 105.0f, 400.0f, 200.0f }, items.c_str(), &setupScroll_,
                    &picked);
        menu_.SelectMap(picked);
        if (const MapEntry *sel = menu_.setup.SelectedMap())
        {
            Art::DrawUiText(&art_, TextFormat("by %s  %s", sel->author.empty() ? "-" : sel->author.c_str(),
                                sel->path.c_str()),
                     static_cast<int>(cx) - 200, 312, 14, GRAY);
        }
        GuiLabel({ cx - 200.0f, 335.0f, 400.0f, 20.0f }, "AI difficulty");
        int diffActive = static_cast<int>(menu_.setup.difficulty);
        const float diffPad = static_cast<float>(GuiGetStyle(TOGGLE, GROUP_PADDING));
        const float diffItemW = (400.0f - diffPad * 2.0f) / 3.0f;
        GuiToggleGroup({ cx - 200.0f, 360.0f, diffItemW, 30.0f }, "Easy;Medium;Hard",
                       &diffActive);
        if (diffActive < 0 || diffActive > 2)
        {
            diffActive = 1;
        }
        menu_.SelectDifficulty(static_cast<AIDifficulty>(diffActive));
        if (!menu_.setup.CanStart())
        {
            GuiDisable();
        }
        if (GuiButton({ cx - 200.0f, 400.0f, 195.0f, 40.0f }, "Start match"))
        {
            if (const MapEntry *sel = menu_.setup.SelectedMap(); sel != nullptr)
            {
                if (menu_.StartMatch())
                {
                    callbacks_.startMatch(sel->path, menu_.setup.difficulty);
                }
            }
        }
        GuiEnable();
        if (GuiButton({ cx + 5.0f, 400.0f, 195.0f, 40.0f }, "Back"))
        {
            Announce(EventType::MenuAction);
            menu_.OpenMainMenu();
        }
    }
    else if (menu_.state == MenuState::Settings)
    {
        Art::DrawUiText(&art_, "Settings", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
        GuiLabel({ cx - 200.0f, 90.0f, 400.0f, 20.0f }, "Camera speed");
        GuiSetTooltip("WASD / edge-pan speed, pixels per second");
        GuiSlider({ cx - 200.0f, 115.0f, 400.0f, 20.0f }, "100", "800",
                  &menu_.settings.cameraSpeed, 100.0f, 800.0f);
        GuiSetTooltip("Show the top-right minimap during matches");
        GuiCheckBox({ cx - 200.0f, 145.0f, 20.0f, 20.0f }, "Minimap",
                    &menu_.settings.showMinimap);
        GuiLabel({ cx - 200.0f, 175.0f, 400.0f, 20.0f }, "Master volume");
        GuiSetTooltip("Scales music and SFX together");
        GuiSlider({ cx - 200.0f, 200.0f, 400.0f, 20.0f }, "0", "1",
                  &menu_.settings.masterVolume, 0.0f, 1.0f);
        GuiLabel({ cx - 200.0f, 230.0f, 400.0f, 20.0f }, "Music volume");
        GuiSetTooltip("Background music level");
        GuiSlider({ cx - 200.0f, 255.0f, 400.0f, 20.0f }, "0", "1",
                  &menu_.settings.musicVolume, 0.0f, 1.0f);
        GuiLabel({ cx - 200.0f, 285.0f, 400.0f, 20.0f }, "SFX volume");
        GuiSetTooltip("Order confirmations, hits, and UI clicks");
        GuiSlider({ cx - 200.0f, 310.0f, 400.0f, 20.0f }, "0", "1",
                  &menu_.settings.sfxVolume, 0.0f, 1.0f);
        GuiSetTooltip("Silence all audio (volumes are kept)");
        GuiCheckBox({ cx - 200.0f, 340.0f, 20.0f, 20.0f }, "Mute", &menu_.settings.mute);
        GuiSetTooltip("Pan the camera by holding right-drag (right-click still orders)");
        GuiCheckBox({ cx - 200.0f, 365.0f, 20.0f, 20.0f }, "Right-drag pan",
                    &menu_.settings.rightDragPan);
        GuiSetTooltip("Orange/blue team palette instead of red/blue (applies live)");
        GuiCheckBox({ cx - 200.0f, 390.0f, 20.0f, 20.0f }, "Color-blind mode",
                    &menu_.settings.colorBlindMode);
        art_.SetColorBlindMode(menu_.settings.colorBlindMode);
        GuiLabel({ cx - 200.0f, 412.0f, 400.0f, 20.0f }, "UI scale");
        GuiSetTooltip("Menu/HUD text size (applies live)");
        GuiSlider({ cx - 200.0f, 434.0f, 400.0f, 20.0f }, "0.75", "2",
                  &menu_.settings.uiScale, 0.75f, 2.0f);
        GuiSetStyle(DEFAULT, TEXT_SIZE,
                    static_cast<int>(10 * menu_.settings.uiScale));
        if (GuiButton({ cx - 200.0f, 470.0f, 400.0f, 40.0f }, "Back"))
        {
            SyncHotkeySettings();
            SaveSettings(menu_.settings, kSettingsPath);
            Announce(EventType::MenuAction);
            menu_.OpenMainMenu();
        }
        if (GuiButton({ cx - 200.0f, 520.0f, 400.0f, 30.0f }, "Remap hotkeys..."))
        {
            BeginRemap(MenuState::Settings);
        }
    }
    else if (menu_.state == MenuState::HotkeyRemap)
    {
        DrawRemap(cx);
    }
    else if (menu_.state == MenuState::LoadGame)
    {
        Art::DrawUiText(&art_, "Load game", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
        const std::string slotPaths[4] = { "data/quicksave.ccpb", SaveSlotPath(1),
                                           SaveSlotPath(2), SaveSlotPath(3) };
        const char *slotLabels[4] = { "Quicksave", "Slot 1", "Slot 2", "Slot 3" };
        for (int i = 0; i < 4; ++i)
        {
            std::error_code ec;
            const bool filled =
                std::filesystem::exists(slotPaths[i], ec) && !ec;
            if (!filled)
            {
                GuiDisable();
            }
            if (GuiButton({ cx - 200.0f, static_cast<float>(90 + i * 50), 400.0f, 40.0f },
                          slotLabels[i]))
            {
                callbacks_.loadSlot(slotPaths[i]);
            }
            GuiEnable();
        }
        if (GuiButton({ cx - 200.0f, 300.0f, 400.0f, 40.0f }, "Back"))
        {
            Announce(EventType::MenuAction);
            menu_.OpenMainMenu();
        }
        if (ReplayFrameCount(kReplayDir) <= 0)
        {
            GuiDisable();
        }
        if (GuiButton({ cx - 200.0f, 350.0f, 400.0f, 40.0f }, "Watch last replay"))
        {
            callbacks_.watchReplay();
        }
        GuiEnable();
    }
    else if (menu_.state == MenuState::MapEditor)
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
    if (!modal)
    {
        EndDrawing();
        return ConfirmChoice::None;
    }
    GuiUnlock();
    const ConfirmChoice choice = DrawConfirmDialog(menu_, screenWidth, screenHeight);
    EndDrawing();
    return choice;
}

ConfirmChoice DrawConfirmDialog(const MenuFlow &menu, int screenWidth, int screenHeight)
{
    if (!menu.ConfirmOpen())
    {
        return ConfirmChoice::None;
    }
    const bool quitting = menu.confirm == ConfirmKind::QuitApp;
    const Rectangle box{ screenWidth / 2.0f - 190.0f, screenHeight / 2.0f - 85.0f, 380.0f,
                         170.0f };
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
    ConfirmChoice choice = ConfirmChoice::None;
    if (GuiWindowBox(box, quitting ? "Quit Conflict Converge?" : "Return to main menu?"))
    {
        choice = ConfirmChoice::No;
    }
    GuiLabel({ box.x + 20.0f, box.y + 46.0f, box.width - 40.0f, 20.0f },
             quitting ? "Close the game?" : "Abandon the current match?");
    if (GuiButton({ box.x + 30.0f, box.y + box.height - 52.0f, 140.0f, 34.0f }, "Yes"))
    {
        choice = ConfirmChoice::Yes;
    }
    if (GuiButton({ box.x + box.width - 170.0f, box.y + box.height - 52.0f, 140.0f, 34.0f }, "No"))
    {
        choice = ConfirmChoice::No;
    }
    return choice;
}

void MenuScreens::DrawRemap(float cx)
{
    Art::DrawUiText(&art_, "Remap hotkeys", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
    GuiLabel({ cx - 200.0f, 70.0f, 600.0f, 20.0f },
             remapArming_ >= 0 ? "Press a key for the armed action (Esc cancels)"
                               : "Click a key to rebind it. Digits/Alt (Tier 2) are fixed.");
    const int defCount = NumHotkeyDefs();
    const int perCol = (defCount + 1) / 2;
    int labelW = 0;
    for (int i = 0; i < defCount; ++i)
    {
        labelW = std::max(labelW, GuiGetTextWidth(kHotkeyDefs[i].label));
    }
    labelW += 8;
    constexpr int kKeyBtnW = 130;
    constexpr int kColGap = 32;
    const int colW = labelW + 8 + kKeyBtnW;
    const float listX = cx - static_cast<float>(colW * 2 + kColGap) / 2.0f;
    const float rowsEnd = 100.0f + static_cast<float>(perCol - 1) * 24.0f + 20.0f;
    for (int i = 0; i < defCount; ++i)
    {
        const HotkeyDef &def = kHotkeyDefs[i];
        const float rx =
            listX + static_cast<float>(i / perCol) * static_cast<float>(colW + kColGap);
        const float ry = 100.0f + static_cast<float>(i % perCol) * 24.0f;
        GuiLabel({ rx, ry, static_cast<float>(labelW), 20.0f }, def.label);
        const int effective = hotkeys_.KeyFor(def.action);
        std::string keyText = effective <= 0 ? "-" : GetKeyName(effective);
        if (def.chord)
        {
            keyText = "Shift+" + keyText;
        }
        if (remapArming_ == i)
        {
            keyText = "press a key...";
        }
        if (GuiButton({ rx + static_cast<float>(labelW) + 8.0f, ry,
                        static_cast<float>(kKeyBtnW), 20.0f },
                      keyText.c_str()))
        {
            if (!remapConflictAction_.empty() && remapConflictAction_ == def.action)
            {
                hotkeys_.Rebind(def.action, remapConflictKey_);
                callbacks_.hotkeysRebound();
                SyncHotkeySettings();
                SaveSettings(menu_.settings, kSettingsPath);
                remapConflictAction_.clear();
                remapArming_ = -1;
            }
            else
            {
                remapArming_ = (remapArming_ == i) ? -1 : i;
                remapConflictAction_.clear();
            }
        }
    }
    if (remapArming_ >= 0)
    {
        int pressed = 0;
        int candidate = 0;
        while ((pressed = GetKeyPressed()) != 0)
        {
            if (pressed != KEY_ESCAPE && pressed != KEY_LEFT_SHIFT &&
                pressed != KEY_RIGHT_SHIFT && pressed != KEY_LEFT_CONTROL &&
                pressed != KEY_RIGHT_CONTROL && pressed != KEY_LEFT_ALT &&
                pressed != KEY_RIGHT_ALT)
            {
                candidate = pressed;
                break;
            }
        }
        if (candidate != 0)
        {
            const std::string action = kHotkeyDefs[remapArming_].action;
            const std::optional<std::string> owner = hotkeys_.ActionForKey(candidate);
            if (owner.has_value() && *owner != action)
            {
                remapConflictAction_ = action;
                remapConflictKey_ = candidate;
            }
            else
            {
                hotkeys_.Rebind(action, candidate);
                callbacks_.hotkeysRebound();
                SyncHotkeySettings();
                SaveSettings(menu_.settings, kSettingsPath);
                remapArming_ = -1;
            }
        }
    }
    if (!remapConflictAction_.empty())
    {
        const std::optional<std::string> owner = hotkeys_.ActionForKey(remapConflictKey_);
        GuiLabel({ cx - 350.0f, rowsEnd + 6.0f, 700.0f, 20.0f },
                 TextFormat("'%s' already fires '%s' — click the row again to steal it.",
                            GetKeyName(remapConflictKey_),
                            owner.has_value() ? owner->c_str() : "?"));
    }
    if (GuiButton({ cx - 200.0f, rowsEnd + 32.0f, 400.0f, 40.0f }, "Back"))
    {
        remapArming_ = -1;
        remapConflictAction_.clear();
        Announce(EventType::MenuAction);
        menu_.state = remapReturn_;
    }
}
