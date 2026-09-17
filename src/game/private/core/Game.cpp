// Game.cpp - owns the former main.cpp boot state + frame loop.
// main.cpp is only the entry point (construct, Run, return).

#include "Game.h"

#include "raygui.h" // M1 Goal 4: raygui UI framework (impl TU: src/thirdparty/raygui_impl.c)
#include "Building.h" // M5 Goal 2: demo base/placement on the tile grid
#include "Formation.h" // drag-select squads fan out through formation moves
#include "Hud.h" // M6 Goal 2: raygui resource + selection panels
#include "MapFile.h" // sandbox detection (player spawn without AI spawn)
#include "Pathfinder.h" // M3 Goal 3: right-click orders route around blocks
#include "Selection.h" // M2 Goal 4: mouse selection helpers
#include "Shortcuts.h" // M6 Goal 4: shortcut overlay lines
#include "Unit.h" // M2 Goal 2/4: snapped units with move orders
#include <algorithm> // QoL area-build tile-range min/max
#include <filesystem> // M14: save-slot existence for the load screen
#include <vector>
#include <cmath> // M12: muzzle direction normalization

namespace
{
// M14: setup-screen and HUD difficulty label.
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

Game::Game()
    : map(20, 15)
    , occ(20, 15)
    , factory(registry, resources, events)
    // M8: enemy commander owns team 1 under fair rules. Parked without a
    // base until the first Start/Load re-arms it (BuildSkirmish runs Reset +
    // SetupBase; the load path runs Reset bare since the file fields the AI).
    , ai(registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , allyAI(registry, map, nodes, events, 0, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , enemyAI2(registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , skirmish{ &registry, &resources, &map, &occ, &fog, &nodes,
                &queue,   &factory,   &ai, &allyAI, &enemyAI2, &camera, &rallyPos }
    // M13: shared snapshot for the save-slot bindings.
    , worldState{ &registry, &resources, &map, &camera, &nodes, &fog, &occ }
{
}

void Game::Init()
{
    const int kInitialWidth = 1200;
    const int kInitialHeight = 675;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE); // M13: user-resizable window
    InitWindow(kInitialWidth, kInitialHeight, "raylib basic window");
    SetWindowMinSize(800, 450); // HUD layout assumes at least this
    SetTargetFPS(60);

    // M11: audio device + asset load. The test binary never inits (headless).
    InitAudioDevice();
    audio.Init(IsAudioDeviceReady());
    // M12: sprite + particle renderer. Missing files fall back to the
    // rectangle placeholders the tests exercise headless.
    art.Init(true);
    // Poll state for edge-triggered sounds (placed/depleted counts, attack
    // rate limit, outcome transitions).
    lastBuildingCount = 0;
    lastDepletedCount = 0;
    lastQueueSize = 0; // M14: production-order edge trigger
    attackSfxTimer = 0.0f;
    lastOutcomeState = MenuState::MainMenu; // M14: boot to title
    hasFactory = false; // M13: recomputed per frame, gates queue + panel

    camera.view.offset = { kInitialWidth / 2.0f, kInitialHeight / 2.0f };
    camera.view.rotation = 0.0f;
    camera.view.zoom = 1.0f;
    // M6 Goal 3: menu flow (pause/outcome/settings); camera speed is a
    // live setting, not a constant, so the settings slider can tune it.
    // M14: boots at MainMenu; persisted settings load here (Q86), ignored
    // when the file is missing.
    LoadSettings(menu.settings, kSettingsPath);
    art.SetColorBlindMode(menu.settings.colorBlindMode); // QoL: persisted palette
    ApplyHotkeyOverrides(); // QoL: persisted remaps, before BindShortcuts below

    // M6 Goal 1: minimap texture (top-right, 4:3 like the 20x15 map).
    minimap.Init({ static_cast<float>(kInitialWidth) - 170.0f, 10.0f,
                   160.0f, 120.0f });
    // Unit speed now comes from M3 base stats (Unit::speed, ApplyBaseStats).

    // M14: world objects are boot-level state, but match CONTENT only builds
    // after Start confirms (BuildSkirmish) or a slot loads. Build/Reset refill
    // contents in place, so shortcut lambdas plus the factory/AI reference
    // bindings stay valid across matches.
    // M14: match session — nothing simulates or renders until Start.
    worldActive = false;
    worldIs2v2 = false;
    worldDifficulty = AIDifficulty::Medium;
    worldMapPath.clear(); // map the active match was seeded from
    settingRally = false;
    dragging = false;
    showHints = true; // M6 Goal 4: F1 toggles the shortcut overlay
    setupScroll = 0; // M14: setup-screen map list scroll position

    // M11: production/spawn confirmations (stateless: survives across matches).
    events.Subscribe(EventType::UnitSpawned, [&](const Event &) { audio.Play(SfxId::Confirm); });
    // QoL pings: player-unit losses raise a marker (position rides the
    // lifecycle payload; dynamic_cast guards against bare-Event sources).
    events.Subscribe(EventType::UnitDestroyed, [&](const Event &e) {
        const auto *lifecycle = dynamic_cast<const UnitLifecycleEvent *>(&e);
        if (lifecycle != nullptr && lifecycle->teamID == 0)
        {
            pings.Raise({ lifecycle->position.x + 32.0f, lifecycle->position.y + 32.0f },
                        PingKind::UnitLost);
        }
    });

    BindShortcuts();
}

void Game::Announce(EventType type)
{
    // M14: bare-event announcer for the Q57 game-state + UI event types.
    Event bare;
    bare.type = type;
    events.Dispatch(bare);
}

// M14: (re)start a skirmish from the setup screen: seed the world, reset
// every per-match poll, and announce the match.
MenuFlow &Game::E2EMenu()
{
    return menu;
}

Registry &Game::E2ERegistry()
{
    return registry;
}

TileMap &Game::E2EMap()
{
    return map;
}

bool Game::IsWorldActive() const
{
    return worldActive;
}

// Same path as the setup screen's "Start match" button (Menu.cpp dispatch).
bool Game::E2EStartSelectedMatch()
{
    if (const MapEntry *sel = menu.setup.SelectedMap(); sel != nullptr)
    {
        if (menu.StartMatch())
        {
            StartMatch(sel->path, menu.setup.difficulty);
            return true;
        }
    }
    return false;
}

void Game::E2EQuitToMenu()
{
    QuitToMenu();
}

void Game::StartMatch(const std::string &mapPath, AIDifficulty difficulty)
{
    // Prototype sandbox: player spawn without an AI spawn means terrain
    // plus one squad — no bases, no AI, no win/lose (see BuildSandbox).
    MapData startData;
    sandboxMode = ParseMapFile(mapPath, startData) && !startData.playerSpawns.empty() &&
                  startData.aiSpawns.empty();
    if (sandboxMode)
    {
        BuildSandbox(skirmish, mapPath);
    }
    else
    {
        BuildSkirmish(skirmish, mapPath, difficulty);
    }
    worldMapPath = mapPath;
    worldDifficulty = difficulty;
    worldActive = true;
    worldIs2v2 = !sandboxMode && SpotsForMap(mapPath).is2v2;
    settingRally = false;
    dragging = false;
    lastBuildingCount = 0;
    lastDepletedCount = 0;
    lastQueueSize = 0;
    attackSfxTimer = 0.0f;
    lastOutcomeState = MenuState::Playing;
    hasFactory = false;
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval; // repaint for the new map now
    // QoL snapshot replay: fresh recording for this match (last match only;
    // the directory is cleared and frames renumbered from zero).
    replayRecording = true;
    replayTimer = 0.0f;
    replayIndex = 0;
    replayCount = 0;
    replayCursor = 0;
    replayPlayTimer = 0.0f;
    {
        std::error_code ec;
        std::filesystem::create_directories(kReplayDir, ec);
        for (int i = 0; i < kReplayMaxFrames; ++i)
        {
            std::filesystem::remove(ReplayFramePath(kReplayDir, i), ec);
        }
    }
    Announce(EventType::MenuAction);
    Announce(EventType::MatchStarted);
}

// M14: teardown back to the title (world contents dropped).
void Game::QuitToMenu()
{
    ResetSkirmish(skirmish);
    worldActive = false;
    worldIs2v2 = false;
    sandboxMode = false;
    replayRecording = false; // QoL: keep this match's frames for the viewer
    Announce(EventType::MenuAction);
    menu.OpenMainMenu();
}

void Game::StepReplay(int dir)
{
    if (replayCount <= 0)
    {
        return;
    }
    replayCursor += dir;
    if (replayCursor < 0)
    {
        replayCursor = 0;
    }
    if (replayCursor >= replayCount)
    {
        replayCursor = replayCount - 1;
    }
    if (LoadWorld(worldState, ReplayFramePath(kReplayDir, replayCursor)))
    {
        replayPlayTimer = 0.0f;
        minimap.elapsed = minimap.refreshInterval; // repaint for the frame now
    }
}

bool Game::WatchLastReplay()
{
    const int count = ReplayFrameCount(kReplayDir);
    if (count <= 0)
    {
        return false;
    }
    ResetSkirmish(skirmish);
    if (!LoadWorld(worldState, ReplayFramePath(kReplayDir, 0)))
    {
        return false;
    }
    replayRecording = false;
    replayCount = count;
    replayCursor = 0;
    replayPlayTimer = 0.0f;
    worldActive = true;
    worldIs2v2 = false;
    settingRally = false;
    dragging = false;
    rightDragging = false;
    pendingRightClick = false;
    rightDragDist = 0.0f;
    attackGroundMode = false;
    hasFactory = false;
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval;
    menu.state = MenuState::ReplayViewer;
    Announce(EventType::MenuAction);
    return true;
}

void Game::ApplyHotkeyOverrides()
{
    hotkeys.ClearOverrides();
    for (const auto &override : menu.settings.hotkeyOverrides)
    {
        hotkeys.Rebind(override.first, override.second);
    }
}

void Game::SyncHotkeySettings()
{
    menu.settings.hotkeyOverrides = hotkeys.Overrides();
}

void Game::DrawHotkeyRemap(float cx)
{
    // QoL remappable hotkeys (Tier 1): click a key, press the new one.
    // Stealing a claimed key needs a second click to confirm.
    DrawText("Remap hotkeys", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
    GuiLabel({ cx - 200.0f, 70.0f, 600.0f, 20.0f },
             remapArming >= 0 ? "Press a key for the armed action (Esc cancels)"
                              : "Click a key to rebind it. Digits/Alt (Tier 2) are fixed.");
    for (int i = 0; i < NumHotkeyDefs(); ++i)
    {
        const HotkeyDef &def = kHotkeyDefs[i];
        const float rx = cx - 380.0f + static_cast<float>(i / 13) * 380.0f;
        const float ry = 100.0f + static_cast<float>(i % 13) * 24.0f;
        GuiLabel({ rx, ry, 220.0f, 20.0f }, def.label);
        const int effective = hotkeys.KeyFor(def.action);
        std::string keyText = effective <= 0 ? "-" : GetKeyName(effective);
        if (def.chord)
        {
            keyText = "Shift+" + keyText;
        }
        if (remapArming == i)
        {
            keyText = "press a key...";
        }
        if (GuiButton({ rx + 225.0f, ry, 130.0f, 20.0f }, keyText.c_str()))
        {
            if (!remapConflictAction.empty() && remapConflictAction == def.action)
            {
                // Second click confirms the steal.
                hotkeys.Rebind(def.action, remapConflictKey);
                BindShortcuts();
                SyncHotkeySettings();
                SaveSettings(menu.settings, kSettingsPath);
                remapConflictAction.clear();
                remapArming = -1;
            }
            else
            {
                remapArming = (remapArming == i) ? -1 : i; // click again cancels
                remapConflictAction.clear();
            }
        }
    }
    // Drain this frame's pressed-key queue into the armed capture.
    // Esc/modifiers never bind (Esc cancels via the Back binding).
    if (remapArming >= 0)
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
            const std::string action = kHotkeyDefs[remapArming].action;
            const std::optional<std::string> owner = hotkeys.ActionForKey(candidate);
            if (owner.has_value() && *owner != action)
            {
                remapConflictAction = action;
                remapConflictKey = candidate;
            }
            else
            {
                hotkeys.Rebind(action, candidate);
                BindShortcuts();
                SyncHotkeySettings();
                SaveSettings(menu.settings, kSettingsPath);
                remapArming = -1;
            }
        }
    }
    if (!remapConflictAction.empty())
    {
        const std::optional<std::string> owner = hotkeys.ActionForKey(remapConflictKey);
        GuiLabel({ cx - 200.0f, 424.0f, 700.0f, 20.0f },
                 TextFormat("'%s' already fires '%s' — click the row again to steal it.",
                            GetKeyName(remapConflictKey),
                            owner.has_value() ? owner->c_str() : "?"));
    }
    if (GuiButton({ cx - 200.0f, 450.0f, 400.0f, 40.0f }, "Back"))
    {
        remapArming = -1;
        remapConflictAction.clear();
        Announce(EventType::MenuAction);
        menu.state = remapReturn;
    }
}

void Game::BindShortcuts()
{
    // Idempotent rebuild: Clear first so rebound-away keys leave no stale
    // binding behind (Bind overwrites same-key slots but never clears the
    // old key). Every literal goes through hotkeys.KeyFor — the remap
    // screen + settings file own the effective keys, not these defaults.
    input.shortcuts.Clear();
    // M2 Goal 5 shortcuts, pumped by the M2 Goal 6 InputManager: Esc
    // deselects, Space halts selected units, P pauses (M6 Goal 3),
    // F1 toggles the shortcut overlay (M6 Goal 4).
    input.shortcuts.Bind(hotkeys.KeyFor("ToggleHints"), [&] { showHints = !showHints; });
    input.shortcuts.Bind(hotkeys.KeyFor("TogglePause"), [&] {
        if (menu.state == MenuState::Playing)
        {
            menu.TogglePause();
            Announce(EventType::MenuAction);
            Announce(EventType::MatchPaused);
        }
        else if (menu.state == MenuState::Paused)
        {
            menu.TogglePause();
            Announce(EventType::MenuAction);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Quicksave"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        SaveWorld({ &registry, &resources, &map, &camera, &nodes, &fog, &occ }, "data/quicksave.ccpb");
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Quickload"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        LoadWorld({ &registry, &resources, &map, &camera, &nodes, &fog, &occ }, "data/quicksave.ccpb");
    });
    // M13: named save slots (F6-8 store, Shift+F6-8 recall).
    input.shortcuts.Bind(hotkeys.KeyFor("SaveSlot1"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            SaveWorld(worldState, SaveSlotPath(1));
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SaveSlot2"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            SaveWorld(worldState, SaveSlotPath(2));
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SaveSlot3"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            SaveWorld(worldState, SaveSlotPath(3));
        }
    });
    input.shortcuts.BindChord(hotkeys.KeyFor("LoadSlot1"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            LoadWorld(worldState, SaveSlotPath(1));
        }
    });
    input.shortcuts.BindChord(hotkeys.KeyFor("LoadSlot2"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            LoadWorld(worldState, SaveSlotPath(2));
        }
    });
    input.shortcuts.BindChord(hotkeys.KeyFor("LoadSlot3"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            LoadWorld(worldState, SaveSlotPath(3));
        }
    });
    // M13: order keys act on the current selection. A attack-moves to the
    // cursor, H/G switch stances, V patrols cursor-and-back, R toggles
    // rally-point placement. M14: world keys are dead outside a live match.
    input.shortcuts.Bind(hotkeys.KeyFor("AttackMove"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        // QoL: squad-wide (not just SelectedUnit) + Shift-queues behind
        // the current order instead of replacing it.
        const Vector2 dest = input.MouseWorld(camera);
        const bool queued = input.ShiftDown();
        int acted = 0;
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            if (unit.isSelected)
            {
                IssueOrEnqueue(unit, map, &occ, id, registry.Generation(id), queued,
                               QueuedOrder{ QueuedOrderKind::AttackMove, dest });
                ++acted;
            }
        });
        if (acted > 0)
        {
            audio.Play(SfxId::Confirm);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("StanceHold"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Entity selected = SelectedUnit(registry);
        if (Unit *unit = registry.Get<Unit>(selected))
        {
            SetStance(*unit, Stance::Hold);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("StanceGuard"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Entity selected = SelectedUnit(registry);
        if (Unit *unit = registry.Get<Unit>(selected))
        {
            SetStance(*unit, Stance::Guard);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Patrol"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        // QoL: squad-wide patrol + Shift-queue, same shape as KEY_A above.
        const Vector2 dest = input.MouseWorld(camera);
        const bool queued = input.ShiftDown();
        int acted = 0;
        registry.Each<Unit>([&](Entity id, Unit &unit) {
            if (unit.isSelected)
            {
                IssueOrEnqueue(unit, map, &occ, id, registry.Generation(id), queued,
                               QueuedOrder{ QueuedOrderKind::Patrol, unit.position, dest });
                ++acted;
            }
        });
        if (acted > 0)
        {
            audio.Play(SfxId::Confirm);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Rally"), [&] {
        if (worldActive && menu.state == MenuState::Playing)
        {
            settingRally = !settingRally;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SelectType"), [&] {
        // QoL select-all-of-type: everything of the selected unit's type.
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        const Entity selected = SelectedUnit(registry);
        const Unit *unit = registry.Get<Unit>(selected);
        if (unit != nullptr)
        {
            SelectAllOfType(registry, unit->type, 0, false); // team 0 is the player
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SelectFactories"), [&] {
        // QoL select-all-production: every owned Factory (minimal building
        // selection — highlight ring + panel count, no command UI yet).
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        SelectAllBuildings(registry, BuildingType::Factory, 0); // team 0 is the player
    });
    input.shortcuts.Bind(hotkeys.KeyFor("SlowestSpeed"), [&] {
        // QoL move-at-slowest-speed: formation marches stop outrunning
        // their slowest unit while armed.
        if (worldActive && menu.state == MenuState::Playing)
        {
            moveAtSlowestSpeed = !moveAtSlowestSpeed;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AreaBuild"), [&] {
        // QoL area-build placement flow: toggle, defaulting to Base.
        // While engaged, 1/2/3 picks the type (see the group loop guard),
        // left-click/drag places, right-click or Esc cancels.
        if (worldActive && menu.state == MenuState::Playing)
        {
            if (placingType.has_value())
            {
                placingType.reset();
            }
            else
            {
                placingType = BuildingType::Base;
            }
            placeDragActive = false;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AreaRepair"), [&] {
        // QoL area-repair mode: toggle, then left-drag a repair zone
        // (mirrors area-build's toggle-then-drag shape).
        if (worldActive && menu.state == MenuState::Playing)
        {
            areaRepairMode = !areaRepairMode;
            repairDragActive = false;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AttackGround"), [&] {
        // QoL attack-ground mode: arm shelling; the next right-click fires
        // it (one-shot, see the RightPressed block), Escape cancels.
        if (worldActive && menu.state == MenuState::Playing)
        {
            attackGroundMode = !attackGroundMode;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AutoRetreat"), [&] {
        // QoL auto-retreat opt-in: per-unit, so glass cannons can retreat
        // while tanks hold. Sets the whole selection uniformly (all on
        // unless all are already on, then all off).
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        bool allOn = true;
        registry.Each<Unit>([&](Entity, const Unit &unit) {
            if (unit.isSelected && !unit.autoRetreat)
            {
                allOn = false;
            }
        });
        registry.Each<Unit>([&](Entity, Unit &unit) {
            if (unit.isSelected)
            {
                unit.autoRetreat = !allOn;
            }
        });
    });
    input.shortcuts.Bind(hotkeys.KeyFor("JumpPing"), [&] {
        // QoL: jump the camera to the most recent attack/loss ping.
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        Vector2 pingPos = {};
        if (pings.Latest(pingPos))
        {
            camera.view.target = pingPos;
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("ReplayBack"), [&] {
        // QoL replay viewer: step one frame back (auto-play resumes after).
        if (menu.state == MenuState::ReplayViewer)
        {
            StepReplay(-1);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("ReplayFwd"), [&] {
        // QoL replay viewer: step one frame forward.
        if (menu.state == MenuState::ReplayViewer)
        {
            StepReplay(1);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Back"), [&] {
        // M14: Esc backs out of menu screens; in-match it keeps the M2
        // deselect behavior (Paused resumes).
        if (menu.state == MenuState::HotkeyRemap)
        {
            // Esc never rebinds: it cancels an armed capture, else backs
            // out to wherever the remap screen was entered from.
            if (remapArming >= 0)
            {
                remapArming = -1;
            }
            else
            {
                remapConflictAction.clear();
                menu.state = remapReturn;
            }
            return;
        }
        if (menu.state == MenuState::ReplayViewer)
        {
            QuitToMenu(); // viewer teardown (world was a loaded snapshot)
            return;
        }
        if (!worldActive)
        {
            if (menu.state == MenuState::SkirmishSetup || menu.state == MenuState::Settings ||
                menu.state == MenuState::LoadGame || menu.state == MenuState::MapEditor)
            {
                Announce(EventType::MenuAction);
                menu.OpenMainMenu();
            }
            return;
        }
        if (menu.state == MenuState::Playing)
        {
            attackGroundMode = false; // QoL: Esc also stands down shell mode
            rightDragging = false; // QoL: Esc also cancels a drawn line
            pendingRightClick = false; // QoL: Esc also drops a deferred click
            rightDragDist = 0.0f;
            placingType.reset(); // QoL: Esc also exits placement mode
            placeDragActive = false;
            areaRepairMode = false; // QoL: Esc also stands down repair mode
            repairDragActive = false;
            DeselectAll(registry);
            dragging = false;
        }
        else if (menu.state == MenuState::Paused)
        {
            menu.TogglePause();
            Announce(EventType::MenuAction);
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("Halt"), [&] {
        if (!worldActive || menu.state != MenuState::Playing)
        {
            return;
        }
        registry.Each<Unit>([&](Entity, Unit &unit) {
            if (unit.isSelected)
            {
                unit.hasMoveOrder = false;
                unit.hasPath = false;
                unit.path.clear();
                unit.pathNext = 0;
                unit.target = kInvalidEntity; // M3 Goal 5: halt drops combat too
                unit.attackMove = false; // M13: halt drops attack-move + repair too
                unit.hasRepairOrder = false;
                unit.repairTarget = kInvalidEntity;
                unit.hasAttackGroundOrder = false; // QoL: halt drops shelling too
                unit.speedCapPixelsPerSec = -1.0f; // QoL: halt drops the group cap
                unit.orderQueue.clear(); // QoL: halt drops queued orders too
                unit.phase = AttackPhase::Ready; // M4 Goal 3: halt cancels the telegraph
                unit.velocity = { 0.0f, 0.0f };
                unit.state = UnitState::Idle;
                SnapUnitToTile(unit);
            }
        });
    });
}

void Game::Update()
{
    // M2 Goal 6: single input pump (always runs: P must unpause too).
    // Camera speed is a live menu setting (M6 Goal 3).
    input.Update(camera, menu.settings.cameraSpeed, GetFrameTime());
    // M13: scroll-wheel zoom (clamped in GameCamera) runs even paused.
    // Zoom-out floor follows the world: at min zoom the map exactly fills
    // the screen, so over-zooming can never show void past the edge.
    camera.AdjustZoom(input.WheelDelta());
    // M13: resizable window — refresh live dims, keep the camera centered
    // and the minimap docked top-right (texture size is fixed).
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    camera.view.offset = { screenWidth / 2.0f, screenHeight / 2.0f };
    minimap.screenRect.x = static_cast<float>(screenWidth) - minimap.screenRect.width - 10.0f;
    minimap.screenRect.y = 10.0f;
    // Camera bounds: pan/zoom/minimap jumps never leave the world zone.
    camera.ClampZoomToWorld(static_cast<float>(map.Width()) * cc::TILE_SIZE,
                            static_cast<float>(map.Height()) * cc::TILE_SIZE, screenWidth,
                            screenHeight);
    camera.ClampToMap(static_cast<float>(map.Width()) * cc::TILE_SIZE,
                      static_cast<float>(map.Height()) * cc::TILE_SIZE, screenWidth,
                      screenHeight);

    // M14: menu branch — no world simulates or renders until Start (or a
    // slot load). The match code below runs untouched once worldActive.
    if (!worldActive)
    {
        audio.ApplySettings(menu.settings.masterVolume, menu.settings.musicVolume,
                            menu.settings.sfxVolume, menu.settings.mute);
        audio.UpdateMusic();

        BeginDrawing();
        ClearBackground(RAYWHITE);
        const float cx = screenWidth / 2.0f;
        if (menu.state == MenuState::MainMenu)
        {
            DrawText("CONFLICT CONVERGE", static_cast<int>(cx) - 290, 110, 52, DARKGRAY);
            DrawText("real-time strategy demo", static_cast<int>(cx) - 140, 175, 20, GRAY);
            if (GuiButton({ cx - 130.0f, 245.0f, 260.0f, 40.0f }, "Start Skirmish"))
            {
                menu.OpenSetup(ListMaps("data/maps"));
                Announce(EventType::MenuAction);
            }
            if (GuiButton({ cx - 130.0f, 295.0f, 260.0f, 40.0f }, "Load Game"))
            {
                menu.OpenLoad();
                Announce(EventType::MenuAction);
            }
            if (GuiButton({ cx - 130.0f, 345.0f, 260.0f, 40.0f }, "Settings"))
            {
                menu.OpenSettings();
                Announce(EventType::MenuAction);
            }
            if (GuiButton({ cx - 130.0f, 395.0f, 260.0f, 40.0f }, "Map Editor"))
            {
                // QoL editor: blank 24x18 scratch canvas (shipped-map size),
                // never the live match map (MainMenu implies no live world).
                editorMap.name = "Custom";
                editorMap.author = "";
                editorMap.width = 24;
                editorMap.height = 18;
                editorMap.terrain.assign(static_cast<std::size_t>(24 * 18),
                                         TerrainType::Grass);
                editorMap.nodes.clear();
                editorMap.playerSpawns.clear();
                editorMap.aiSpawns.clear();
                editorBrush = '.';
                editorStatus = "";
                menu.state = MenuState::MapEditor;
                Announce(EventType::MenuAction);
            }
            if (GuiButton({ cx - 130.0f, 445.0f, 260.0f, 40.0f }, "Quit"))
            {
                Announce(EventType::MenuAction);
                menu.quitRequested = true;
            }
        }
        else if (menu.state == MenuState::SkirmishSetup)
        {
            DrawText("Skirmish setup", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
            GuiLabel({ cx - 200.0f, 80.0f, 400.0f, 20.0f }, "Map (from data/*.map)");
            std::string items;
            for (const MapEntry &entry : menu.setup.maps)
            {
                items += entry.name + " (" + std::to_string(entry.width) + "x" +
                         std::to_string(entry.height) + ");";
            }
            if (items.empty())
            {
                items = "<no maps found>;";
            }
            int picked = menu.setup.mapIndex;
            GuiListView({ cx - 200.0f, 105.0f, 400.0f, 200.0f }, items.c_str(),
                        &setupScroll, &picked);
            menu.SelectMap(picked);
            if (const MapEntry *sel = menu.setup.SelectedMap())
            {
                DrawText(TextFormat("by %s  %s", sel->author.empty() ? "-" : sel->author.c_str(),
                                    sel->path.c_str()),
                         static_cast<int>(cx) - 200, 312, 14, GRAY);
            }
            GuiLabel({ cx - 200.0f, 335.0f, 400.0f, 20.0f }, "AI difficulty");
            int diffActive = static_cast<int>(menu.setup.difficulty);
            // raygui GuiToggleGroup bounds.width is per-item unless
            // GROUP_WIDTH_FULL=1 (default 0): 400px would make each of the
            // 3 toggles 400px wide (1200px total, overflowing the panel).
            // Fit 3 items exactly in the 400px panel (local qwen verified: 132).
            const float diffPad = static_cast<float>(GuiGetStyle(TOGGLE, GROUP_PADDING));
            const float diffItemW = (400.0f - diffPad * 2.0f) / 3.0f;
            GuiToggleGroup({ cx - 200.0f, 360.0f, diffItemW, 30.0f }, "Easy;Medium;Hard",
                           &diffActive);
            if (diffActive < 0 || diffActive > 2)
            {
                diffActive = 1;
            }
            menu.SelectDifficulty(static_cast<AIDifficulty>(diffActive));
            if (!menu.setup.CanStart())
            {
                GuiDisable();
            }
            if (GuiButton({ cx - 200.0f, 400.0f, 195.0f, 40.0f }, "Start match"))
            {
                if (const MapEntry *sel = menu.setup.SelectedMap(); sel != nullptr)
                {
                    if (menu.StartMatch())
                    {
                        StartMatch(sel->path, menu.setup.difficulty);
                    }
                }
            }
            GuiEnable();
            if (GuiButton({ cx + 5.0f, 400.0f, 195.0f, 40.0f }, "Back"))
            {
                Announce(EventType::MenuAction);
                menu.OpenMainMenu();
            }
        }
        else if (menu.state == MenuState::Settings)
        {
            DrawText("Settings", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
            GuiLabel({ cx - 200.0f, 90.0f, 400.0f, 20.0f }, "Camera speed");
            GuiSlider({ cx - 200.0f, 115.0f, 400.0f, 20.0f }, "100", "800",
                      &menu.settings.cameraSpeed, 100.0f, 800.0f);
            GuiCheckBox({ cx - 200.0f, 145.0f, 20.0f, 20.0f }, "Minimap",
                        &menu.settings.showMinimap);
            GuiLabel({ cx - 200.0f, 175.0f, 400.0f, 20.0f }, "Master volume");
            GuiSlider({ cx - 200.0f, 200.0f, 400.0f, 20.0f }, "0", "1",
                      &menu.settings.masterVolume, 0.0f, 1.0f);
            GuiLabel({ cx - 200.0f, 230.0f, 400.0f, 20.0f }, "Music volume");
            GuiSlider({ cx - 200.0f, 255.0f, 400.0f, 20.0f }, "0", "1",
                      &menu.settings.musicVolume, 0.0f, 1.0f);
            GuiLabel({ cx - 200.0f, 285.0f, 400.0f, 20.0f }, "SFX volume");
            GuiSlider({ cx - 200.0f, 310.0f, 400.0f, 20.0f }, "0", "1",
                      &menu.settings.sfxVolume, 0.0f, 1.0f);
            GuiCheckBox({ cx - 200.0f, 340.0f, 20.0f, 20.0f }, "Mute", &menu.settings.mute);
            GuiCheckBox({ cx - 200.0f, 365.0f, 20.0f, 20.0f }, "Right-drag pan",
                        &menu.settings.rightDragPan);
            GuiCheckBox({ cx - 200.0f, 390.0f, 20.0f, 20.0f }, "Color-blind mode",
                        &menu.settings.colorBlindMode);
            art.SetColorBlindMode(menu.settings.colorBlindMode); // live, no reopen needed
            if (GuiButton({ cx - 200.0f, 425.0f, 400.0f, 40.0f }, "Back"))
            {
                SyncHotkeySettings(); // QoL: remaps ride the settings file too
                SaveSettings(menu.settings, kSettingsPath); // M14: Q86 persistence
                Announce(EventType::MenuAction);
                menu.OpenMainMenu();
            }
            if (GuiButton({ cx - 200.0f, 475.0f, 400.0f, 30.0f }, "Remap hotkeys..."))
            {
                remapArming = -1;
                remapConflictAction.clear();
                remapReturn = MenuState::Settings;
                Announce(EventType::MenuAction);
                menu.state = MenuState::HotkeyRemap;
            }
        }
        else if (menu.state == MenuState::HotkeyRemap)
        {
            DrawHotkeyRemap(cx);
        }
        else if (menu.state == MenuState::LoadGame)
        {
            DrawText("Load game", static_cast<int>(cx) - 200, 40, 28, DARKGRAY);
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
                    // M14: load over a fresh shell (LoadWorld clears +
                    // rebuilds). The file already fields the AI side, so
                    // the commander re-arms bare — no second SetupBase.
                    ResetSkirmish(skirmish);
                    if (LoadWorld(worldState, slotPaths[i]))
                    {
                        const SkirmishSpots spots = SpotsForMap(
                            worldMapPath.empty() ? "data/maps/crossroads.map" : worldMapPath);
                        ai.Reset(menu.setup.difficulty, spots.aiHome, spots.playerHome, 1);
                        if (spots.is2v2)
                        {
                            // 2v2 save: the file already fields all three AI
                            // sides, so all commanders re-arm bare.
                            allyAI.Reset(menu.setup.difficulty, spots.allyHome, spots.aiHome, 0);
                            enemyAI2.Reset(menu.setup.difficulty, spots.enemyHome2,
                                           spots.playerHome, 1);
                        }
                        rallyPos = camera.view.target;
                        worldDifficulty = menu.setup.difficulty;
                        worldActive = true;
                        worldIs2v2 = spots.is2v2;
                        settingRally = false;
                        dragging = false;
                        lastBuildingCount = 0;
                        lastDepletedCount = 0;
                        lastQueueSize = 0;
                        attackSfxTimer = 0.0f;
                        lastOutcomeState = MenuState::Playing;
                        hasFactory = false;
                        camera.view.zoom = 1.0f;
                        minimap.elapsed = minimap.refreshInterval;
                        menu.state = MenuState::Playing;
                        Announce(EventType::MenuAction);
                        Announce(EventType::MatchStarted);
                    }
                }
                GuiEnable();
            }
            if (GuiButton({ cx - 200.0f, 300.0f, 400.0f, 40.0f }, "Back"))
            {
                Announce(EventType::MenuAction);
                menu.OpenMainMenu();
            }
            // QoL snapshot replay entry: enabled when the last match left
            // frames behind. Loads frame 0 and freezes the sim (viewer).
            if (ReplayFrameCount(kReplayDir) <= 0)
            {
                GuiDisable();
            }
            if (GuiButton({ cx - 200.0f, 350.0f, 400.0f, 40.0f }, "Watch last replay"))
            {
                WatchLastReplay();
            }
            GuiEnable();
        }
        else if (menu.state == MenuState::MapEditor)
        {
            // QoL tile painter on scratch state (never the live match).
            // Left-drag paints the brush, right-click erases to grass,
            // keys 1-9 switch brush. Save validates + warns, never blocks
            // on playability (see ValidateMapPlayable).
            constexpr float kCell = 24.0f, kOx = 20.0f, kOy = 100.0f;
            DrawText("Map Editor - data/maps/custom.map", 20, 40, 24, DARKGRAY);
            const char kBrushes[9] = { '.', '~', 'T', '^', 'B', 'I', 'O', '1', '2' };
            const int kPaletteKeys[9] = { KEY_ONE, KEY_TWO,   KEY_THREE, KEY_FOUR, KEY_FIVE,
                                          KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE };
            for (int i = 0; i < 9; ++i)
            {
                if (IsKeyPressed(kPaletteKeys[i]))
                {
                    editorBrush = kBrushes[i];
                }
            }
            const Vector2 mouse = input.MouseScreen();
            const cc::IVec2 hover{ static_cast<int>((mouse.x - kOx) / kCell),
                                   static_cast<int>((mouse.y - kOy) / kCell) };
            if (input.LeftDown())
            {
                PaintEditorCell(editorMap, editorBrush, hover);
            }
            if (input.RightPressed())
            {
                PaintEditorCell(editorMap, '.', hover);
            }
            for (int y = 0; y < editorMap.height; ++y)
            {
                for (int x = 0; x < editorMap.width; ++x)
                {
                    const TerrainType terrain = editorMap
                                                    .terrain[static_cast<std::size_t>(y) *
                                                                 editorMap.width +
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
            // Marker letters over their tiles.
            for (const MapNodeSpawn &spawn : editorMap.nodes)
            {
                DrawText(spawn.kind == ResourceKind::Iron ? "I" : "O",
                         static_cast<int>(kOx + spawn.tile.x * kCell) + 7,
                         static_cast<int>(kOy + spawn.tile.y * kCell) + 3, 16, WHITE);
            }
            for (const cc::IVec2 &tile : editorMap.playerSpawns)
            {
                DrawText("1", static_cast<int>(kOx + tile.x * kCell) + 7,
                         static_cast<int>(kOy + tile.y * kCell) + 3, 16, WHITE);
            }
            for (const cc::IVec2 &tile : editorMap.aiSpawns)
            {
                DrawText("2", static_cast<int>(kOx + tile.x * kCell) + 7,
                         static_cast<int>(kOy + tile.y * kCell) + 3, 16, WHITE);
            }
            DrawText(TextFormat("Brush: %c (keys 1-9)", editorBrush), 660, 100, 16,
                     DARKGRAY);
            DrawText("Left-drag paints, right-click erases", 660, 124, 14, GRAY);
            DrawText("Maps need '1', '2' and a connecting path", 660, 142, 14, GRAY);
            if (GuiButton({ 660.0f, 200.0f, 200.0f, 40.0f }, "Save"))
            {
                if (WriteMapFile(editorMap, "data/maps/custom.map"))
                {
                    std::string warning;
                    editorStatus = ValidateMapPlayable(editorMap, &warning)
                                       ? "Saved to data/maps/custom.map"
                                       : "Saved with warnings: " + warning;
                }
                else
                {
                    editorStatus = "Save failed (invalid dims)";
                }
            }
            if (GuiButton({ 660.0f, 250.0f, 200.0f, 40.0f }, "Back"))
            {
                menu.OpenMainMenu();
            }
            DrawText(editorStatus.c_str(), 660, 300, 14, DARKGREEN);
        }
        else
        {
            // Unreachable (match states always carry a world); recover.
            menu.OpenMainMenu();
        }
        EndDrawing();
        return;
    }

    // M6 Goal 3: orders, AI, economy, and minimap only advance while
    // Playing; rendering below always runs so menus overlay a live frame.
    if (menu.state == MenuState::Playing)
    {

        // M2 Goal 4 mouse inputs: press starts a drag-box gesture,
        // release resolves it (click = pick, box = SelectInRect).
        // M3 Goal 3: orders pathfind around water/buildings via IssuePathOrder.
        // M13 routes minimap clicks to the camera and rally-mode clicks
        // to the factory rally point before unit selection.
        if (input.LeftPressed())
        {
            if (placingType.has_value())
            {
                // QoL area-build: press starts a placement drag (click =
                // single footprint, drag = tiled pattern on release).
                placeDragActive = true;
                placeDragStart = input.MouseScreen();
            }
            else if (areaRepairMode)
            {
                // QoL area-repair: press starts a repair-zone drag (click =
                // tiny rect, drag = repair zone on release). Checked ahead
                // of box-select, same as placingType above.
                repairDragActive = true;
                repairDragStart = input.MouseScreen();
            }
            else if (minimap.Contains(input.MouseScreen()))
            {
                camera.view.target =
                    minimap.MinimapToWorld(input.MouseScreen(), map.Width(), map.Height());
            }
            else if (settingRally)
            {
                rallyPos = input.MouseWorld(camera);
                settingRally = false;
            }
            else
            {
                dragging = true;
                dragStart = input.MouseScreen();
            }
        }
        if (dragging && !input.LeftDown())
        {
            dragging = false;
            const Rectangle box = NormalizeRect(dragStart, input.MouseScreen());
            if (box.width < 6.0f && box.height < 6.0f)
            {
                const Vector2 world = input.MouseWorld(camera);
                const Entity hit = PickUnitAt(registry, world);
                if (hit != kInvalidEntity)
                {
                    const Unit *hitUnit = registry.Get<Unit>(hit);
                    // QoL double-click: same type + team across the viewport
                    // instead of the single pick. PickUnitAt has no team
                    // filter, so double-clicking an enemy resolves here but
                    // the team-0 filter below selects nothing (acceptable:
                    // enemy units aren't commandable anyway).
                    if (hitUnit != nullptr && input.DoubleClicked())
                    {
                        SelectAllOfTypeInRect(
                            registry,
                            DraggedWorldBox(camera, { 0.0f, 0.0f },
                                            { static_cast<float>(GetScreenWidth()),
                                              static_cast<float>(GetScreenHeight()) }),
                            hitUnit->type, 0, false);
                    }
                    else
                    {
                        SelectOnly(registry, hit);
                    }
                    audio.Play(SfxId::Select); // M11: selection blip
                }
                else
                {
                    DeselectAll(registry);
                }
            }
            else
            {
                // Screen box corners back to world space (Shift extends).
                if (SelectInRect(registry,
                                 DraggedWorldBox(camera, { box.x, box.y },
                                                 { box.x + box.width, box.y + box.height }),
                                 input.ShiftDown()) > 0)
                {
                    audio.Play(SfxId::Select);
                }
            }
        }
        // QoL area-build release: click places one footprint, a drag tiles
        // the footprint across the box (invalid slots skipped silently).
        // The mode stays engaged for rows of structures; right-click/Esc
        // exits (see the RightPressed block and the Esc binding).
        if (placeDragActive && !input.LeftDown())
        {
            placeDragActive = false;
            if (placingType.has_value())
            {
                const Rectangle box =
                    DraggedWorldBox(camera, placeDragStart, input.MouseScreen());
                if (box.width < 6.0f && box.height < 6.0f)
                {
                    const cc::IVec2 tile =
                        cc::WorldToTile(cc::ToGlm(input.MouseWorld(camera)));
                    if (PlaceBuilding(registry, map, *placingType, 0, tile.x, tile.y,
                                      &nodes) != kInvalidEntity)
                    {
                        audio.Play(SfxId::Place);
                    }
                }
                else
                {
                    const cc::IVec2 a =
                        cc::WorldToTile(cc::ToGlm({ box.x, box.y }));
                    const cc::IVec2 b = cc::WorldToTile(
                        cc::ToGlm({ box.x + box.width, box.y + box.height }));
                    int placed = 0;
                    for (const cc::IVec2 &slot :
                         AreaBuildSlots(*placingType, { std::min(a.x, b.x),
                                                        std::min(a.y, b.y) },
                                        { std::max(a.x, b.x), std::max(a.y, b.y) }))
                    {
                        if (PlaceBuilding(registry, map, *placingType, 0, slot.x, slot.y,
                                          &nodes) != kInvalidEntity)
                        {
                            ++placed;
                        }
                    }
                    if (placed > 0)
                    {
                        audio.Play(SfxId::Place);
                    }
                }
            }
        }
        // QoL area-repair release: collect damaged candidates in the zone,
        // greedily assign each selected Engineer its nearest unclaimed one.
        // Click-sized drags resolve as a tiny rect (usually a silent no-op).
        // The mode stays engaged for repeat drags; right-click/Esc exits.
        if (repairDragActive && !input.LeftDown())
        {
            repairDragActive = false;
            if (areaRepairMode)
            {
                const Rectangle zone =
                    DraggedWorldBox(camera, repairDragStart, input.MouseScreen());
                std::vector<Entity> engineers;
                registry.Each<Unit>([&](Entity id, const Unit &unit) {
                    if (unit.isSelected && unit.type == UnitType::Engineer)
                    {
                        engineers.push_back(id);
                    }
                });
                std::vector<Entity> candidates;
                CollectAreaRepairCandidates(registry, zone, 0, candidates);
                std::vector<RepairAssignment> assignments;
                AssignAreaRepair(registry, engineers, candidates, assignments);
                for (const RepairAssignment &job : assignments)
                {
                    if (Unit *engineer = registry.Get<Unit>(job.engineer))
                    {
                        IssueOrEnqueue(*engineer, map, &occ, job.engineer,
                                       registry.Generation(job.engineer), input.ShiftDown(),
                                       QueuedOrder{ QueuedOrderKind::Repair, {}, {},
                                                    job.target });
                    }
                }
                if (!assignments.empty())
                {
                    audio.Play(SfxId::Confirm);
                }
            }
        }
        const bool altDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        // QoL right-drag pan (opt-in setting): accumulate held distance
        // every frame; past the click-vs-drag threshold the camera grabs
        // the world (mirrors left-click's 6px click-vs-box rule).
        if (input.RightDown() && !altDown)
        {
            const Vector2 panDelta = input.MouseDeltaScreen();
            rightDragDist += std::sqrt(panDelta.x * panDelta.x + panDelta.y * panDelta.y);
            if (menu.settings.rightDragPan && rightDragDist > 6.0f)
            {
                const float zoom = camera.view.zoom <= 0.0f ? 1.0f : camera.view.zoom;
                camera.Pan({ -panDelta.x / zoom, -panDelta.y / zoom });
            }
        }
        // Whether this press cancelled placement (orders stay silent then).
        const bool wasPlacing = placingType.has_value() || areaRepairMode;
        if (input.RightPressed())
        {
            // QoL area-build: right-click cancels placement (no order).
            if (placingType.has_value())
            {
                placingType.reset();
                placeDragActive = false;
            }
            // QoL area-repair: right-click stands the mode down (no order).
            // Chained (not separate) so Alt+right-drag can't arm a line
            // while either mode cancels, exactly like placement before it.
            else if (areaRepairMode)
            {
                areaRepairMode = false;
                repairDragActive = false;
            }
            // QoL line formation: Alt+right-drag draws a placement line
            // (Ctrl is groups, Shift is queueing — Alt stays unambiguous).
            // No order fires on the press itself; the release dispatches.
            else if (altDown)
            {
                rightDragging = true;
                rightDragStart = input.MouseScreen();
            }
        }
        const bool cancelledPlacement = input.RightPressed() && wasPlacing;
        // and the deferred release path (option on: click-vs-drag is only
        // known on release, where the mouse still sits ~at the press point).
        auto dispatchRightClickOrders = [&]() {
            // QoL attack-ground mode (toggled with X): the next right-click
            // shells the clicked point instead of moving there. One-shot:
            // the mode clears after a single use.
            if (attackGroundMode)
            {
                attackGroundMode = false;
                const Vector2 target = input.MouseWorld(camera);
                const bool queued = input.ShiftDown();
                registry.Each<Unit>([&](Entity id, Unit &unit) {
                    if (unit.isSelected)
                    {
                        IssueOrEnqueue(unit, map, &occ, id, registry.Generation(id), queued,
                                       QueuedOrder{ QueuedOrderKind::AttackGround, target });
                    }
                });
                audio.Play(SfxId::Confirm);
            }
            else
            {
            // Single selection keeps the direct path order; groups fan
            // out through the formation move.
            std::vector<Entity> squad;
            registry.Each<Unit>([&](Entity id, const Unit &unit) {
                if (unit.isSelected)
                {
                    squad.push_back(id);
                }
            });
            if (squad.size() == 1)
            {
                if (Unit *ordered = registry.Get<Unit>(squad[0]))
                {
                    // QoL single-target repair: a lone selected Engineer
                    // right-clicked onto a damaged same-team unit/building
                    // repairs instead of moving (Shift queues it behind the
                    // current order via the same path as other orders).
                    bool repaired = false;
                    if (ordered->type == UnitType::Engineer)
                    {
                        const Vector2 world = input.MouseWorld(camera);
                        Entity patient = PickUnitAt(registry, world);
                        if (patient == kInvalidEntity ||
                            !CanRepairTarget(registry, *ordered, patient))
                        {
                            patient = kInvalidEntity;
                            const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(world));
                            registry.Each<Building>([&](Entity id, const Building &building) {
                                if (patient != kInvalidEntity)
                                {
                                    return;
                                }
                                // Shared footprint-rect helper (also feeds
                                // area repair) instead of inline tile math.
                                const Rectangle footprint = BuildingFootprintRect(building);
                                const Vector2 tileCenter = cc::ToRaylib(
                                    cc::TileToWorld(tile.x, tile.y) + cc::Vec2(32.0f, 32.0f));
                                if (CheckCollisionPointRec(tileCenter, footprint) &&
                                    CanRepairTarget(registry, *ordered, id))
                                {
                                    patient = id;
                                }
                            });
                        }
                        if (patient != kInvalidEntity)
                        {
                            IssueOrEnqueue(*ordered, map, &occ, squad[0],
                                           registry.Generation(squad[0]), input.ShiftDown(),
                                           QueuedOrder{ QueuedOrderKind::Repair, {}, {}, patient });
                            repaired = true;
                        }
                    }
                    if (!repaired)
                    {
                        if (input.ShiftDown())
                        {
                            // QoL: queue behind the current order.
                            IssueOrEnqueue(*ordered, map, &occ, squad[0],
                                           registry.Generation(squad[0]), true,
                                           QueuedOrder{ QueuedOrderKind::Move,
                                                        input.MouseWorld(camera) });
                        }
                        else
                        {
                            ordered->orderQueue.clear();
                            // Fresh single order (not formation): ClearOrders
                            // resets every order-type flag — IssuePathOrder*
                            // doesn't route through ClearOrders like the
                            // Issue* wrappers (see ClearOrders in Unit.h).
                            ClearOrders(*ordered);
                            IssuePathOrderFootprint(*ordered, map, occ,
                                                    input.MouseWorld(camera), squad[0],
                                                    registry.Generation(squad[0]));
                        }
                    }
                    audio.Play(SfxId::Confirm); // M11: order acknowledged
                }
            }
            else if (!squad.empty())
            {
                if (input.ShiftDown())
                {
                    // QoL: queue the same destination per unit (no fan-out
                    // for queued legs — formation applies to live orders).
                    const Vector2 dest = input.MouseWorld(camera);
                    for (const Entity id : squad)
                    {
                        if (Unit *unit = registry.Get<Unit>(id))
                        {
                            IssueOrEnqueue(*unit, map, &occ, id, registry.Generation(id),
                                           true,
                                           QueuedOrder{ QueuedOrderKind::Move, dest });
                        }
                    }
                }
                else
                {
                    // IssueFormationMoveFP does its own fresh-order
                    // bookkeeping (flag + queue clear), like the
                    // line-formation variant.
                    formation::IssueFormationMoveFP(registry, squad, map, occ,
                                                    input.MouseWorld(camera),
                                                    moveAtSlowestSpeed);
                }
                audio.Play(SfxId::Confirm);
            }
            }
        };
        if (input.RightPressed() && !rightDragging && !cancelledPlacement)
        {
            if (menu.settings.rightDragPan && !altDown)
            {
                pendingRightClick = true; // decided on release, below
            }
            else
            {
                dispatchRightClickOrders();
            }
        }
        if (!input.RightDown())
        {
            // Release with the option on: a sub-threshold press was a click
            // after all — run the deferred order now (~at the press point).
            if (pendingRightClick && rightDragDist < 6.0f)
            {
                dispatchRightClickOrders();
            }
            pendingRightClick = false;
            rightDragDist = 0.0f;
        }
        // QoL line formation release: order the current squad along the
        // drawn line, then stand down the gesture.
        if (rightDragging && !input.RightDown())
        {
            rightDragging = false;
            std::vector<Entity> squad;
            registry.Each<Unit>([&](Entity id, const Unit &unit) {
                if (unit.isSelected)
                {
                    squad.push_back(id);
                }
            });
            if (!squad.empty())
            {
                const auto [lineStart, lineEnd] =
                    DraggedWorldLine(camera, rightDragStart, input.MouseScreen());
                formation::IssueLineFormationMoveFP(registry, squad, map, occ, lineStart,
                                                    lineEnd, moveAtSlowestSpeed);
                audio.Play(SfxId::Confirm);
            }
        }
        // QoL control groups: number keys recall, Ctrl+number assigns the
        // current selection (replacing), Shift+number adds to it,
        // Ctrl+Shift+number routes future production into it. Polled here
        // (not via ShortcutRegistry) because one key needs 4-way
        // modifier disambiguation the registry's plain/Shift-chord model
        // can't express. Displayed 1-9,0 for bits 0-9.
        static constexpr int kGroupKeys[10] = { KEY_ONE,   KEY_TWO,   KEY_THREE, KEY_FOUR,
                                                KEY_FIVE,  KEY_SIX,   KEY_SEVEN, KEY_EIGHT,
                                                KEY_NINE,  KEY_ZERO };
        for (int bit = 0; bit < 10; ++bit)
        {
            if (!IsKeyPressed(kGroupKeys[bit]) || placingType.has_value())
            {
                continue; // 1/2/3 steer the placement type while placing
            }
            if (input.CtrlDown() && input.ShiftDown())
            {
                autoAddGroupBit = bit;
            }
            else if (input.CtrlDown())
            {
                AssignControlGroup(registry, bit);
            }
            else if (input.ShiftDown())
            {
                AddToControlGroup(registry, bit);
            }
            else
            {
                RecallControlGroup(registry, bit);
            }
        }
        // QoL area-build: 1/2/3 picks the placement type while engaged
        // (the group loop above stands down for those keys meanwhile).
        if (placingType.has_value())
        {
            if (IsKeyPressed(KEY_ONE))
            {
                placingType = BuildingType::Base;
            }
            else if (IsKeyPressed(KEY_TWO))
            {
                placingType = BuildingType::ResourceDepot;
            }
            else if (IsKeyPressed(KEY_THREE))
            {
                placingType = BuildingType::Factory;
            }
        }
        // M9: rebuild visibility from current positions before anyone acquires.
        fog.Recompute(registry);
        // Phase 4 + stall-fix: occupancy pre-pass, per-unit driver (M3G5,
        // M9 fog gate), overlap separation, and separation-stall detection,
        // all as one pipeline (see RunUnitMovementFrame) so the game loop
        // and tests can't drift apart on this sequencing.
        RunUnitMovementFrame(registry, map, occ, &fog, GetFrameTime());
        // M3 Goal 6: collect the fallen, then destroy through the factory so
        // UnitDestroyed is announced (destroying inside Each would invalidate it).
        // dt is shared by the M11 polls below and the economy tick further down.
        const float dt = GetFrameTime();
        std::vector<Entity> dead;
        registry.Each<Unit>([&](Entity id, const Unit &unit) {
            if (unit.health <= 0.0f)
            {
                dead.push_back(id);
            }
        });
        for (Entity id : dead)
        {
            if (const Unit *corpse = registry.Get<Unit>(id))
            {
                // M12: death burst at the corpse before teardown.
                art.ParticlesPool().SpawnBurst(
                    { corpse->position.x + 32.0f, corpse->position.y + 32.0f }, ORANGE, 24,
                    120.0f, 0.6f);
                // Phase 4: release occupancy tiles before teardown (owned:
                // a corpse shoved onto a live unit's tile must not wipe it).
                const cc::IVec2 anchor = cc::WorldToTile(cc::ToGlm(corpse->position));
                occ.ReleaseFootprintOwned(anchor, corpse->footprintWidth,
                                          corpse->footprintHeight, id,
                                          registry.Generation(id));
            }
            factory.DestroyUnit(id);
        }
        // M11: edge-triggered battle sounds (one explosion per wipe, not per corpse).
        if (!dead.empty())
        {
            audio.Play(SfxId::Explosion);
        }
        // M12: impact sparks on fresh hits + muzzle sparks while telegraphing.
        registry.Each<Unit>([&](Entity, const Unit &unit) {
            const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
            if (unit.hitFlashTime > 0.20f)
            {
                art.ParticlesPool().SpawnBurst(center, YELLOW, 6, 90.0f, 0.25f);
                // QoL pings: same "just got hit" detector, no Combat.h
                // changes (per-kind floor inside Pings stops spam).
                // Team 0 is the player.
                if (unit.teamID == 0)
                {
                    pings.Raise(center, PingKind::UnderAttack);
                }
            }
            if (unit.phase == AttackPhase::WindUp)
            {
                if (const Unit *target = registry.Get<Unit>(unit.target))
                {
                    const Vector2 dir = { target->position.x - unit.position.x,
                                          target->position.y - unit.position.y };
                    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len > 1.0f)
                    {
                        art.ParticlesPool().SpawnBurst(
                            { center.x + dir.x / len * 20.0f, center.y + dir.y / len * 20.0f },
                            ORANGE, 2, 40.0f, 0.15f);
                    }
                }
            }
        });
        art.ParticlesPool().Update(dt);
        attackSfxTimer -= dt;
        bool windingUp = false;
        registry.Each<Unit>([&](Entity, const Unit &unit) {
            if (unit.phase == AttackPhase::WindUp)
            {
                windingUp = true;
            }
        });
        if (windingUp && attackSfxTimer <= 0.0f)
        {
            audio.Play(SfxId::Attack);
            attackSfxTimer = 0.12f;
        }
        int buildingCount = 0;
        registry.Each<Building>([&](Entity, const Building &building) {
            if (building.state == BuildingState::Operational)
            {
                ++buildingCount;
            }
        });
        if (buildingCount > lastBuildingCount)
        {
            audio.Play(SfxId::Place);
        }
        lastBuildingCount = buildingCount;
        int depletedCount = 0;
        nodes.Each([&](const ResourceNode &node) {
            if (node.IsDepleted())
            {
                ++depletedCount;
            }
        });
        if (depletedCount > lastDepletedCount)
        {
            audio.Play(SfxId::Deplete);
            Announce(EventType::ResourceDepleted); // M14: Q57 resource event
        }
        lastDepletedCount = depletedCount;
        // M5 economy tick: base trickle, node respawn, harvest, production.
        // (dt is declared up at the death sweep so the M11 polls above share it.)
        UpdateBaseIncome(registry, resources, dt, 0);
        nodes.Update(dt);
        nodes.GatherTick(registry, resources, dt, 0); // team 0 crew only (AI gathers its own)
        // M13: production dies with the structure — compute here for the
        // queue gate, reuse for the factory panel below.
        hasFactory = false;
        registry.Each<Building>([&](Entity, const Building &building) {
            if (building.teamID == 0 && building.type == BuildingType::Factory &&
                building.state == BuildingState::Operational)
            {
                hasFactory = true;
            }
        });
        if (hasFactory)
        {
            const Entity spawned = queue.Update(factory, resources, 0, rallyPos, dt);
            if (spawned != kInvalidEntity && autoAddGroupBit >= 0)
            {
                if (Unit *fresh = registry.Get<Unit>(spawned))
                {
                    fresh->controlGroups |= (1u << static_cast<unsigned int>(autoAddGroupBit));
                }
            }
        }
        // M14: Q57 production-ordered event on every queue growth (panel
        // buttons and the M5 seed enqueues both flow through here).
        if (queue.Size() > static_cast<std::size_t>(lastQueueSize))
        {
            Announce(EventType::ProductionOrdered);
        }
        lastQueueSize = static_cast<int>(queue.Size());
        // QoL snapshot replay: capture a full-state frame every 2s while
        // the match runs (best-effort: a failed write retries next tick
        // without consuming the frame number; stops at the frame cap).
        if (replayRecording)
        {
            replayTimer += dt;
            if (replayTimer >= 2.0f)
            {
                replayTimer = 0.0f;
                if (replayIndex < kReplayMaxFrames &&
                    SaveWorld(worldState, ReplayFramePath(kReplayDir, replayIndex)))
                {
                    ++replayIndex;
                    replayCount = replayIndex;
                }
            }
        }
        // QoL building auto-repair (player team 0 only; AI economy is
        // soak-tuned and Engineers already repair free — see Building.h).
        if (playerAutoRepair)
        {
            UpdateBuildingAutoRepair(registry, resources, dt, 0, autoRepairCap);
        }
        // QoL player auto-retreat: opted-in units below threshold fall back
        // to the rally point (or the nearest owned Base when no rally was
        // ever placed). Shared RetreatIfLowHP with the AI's RetreatTick.
        {
            const Vector2 home = ResolvePlayerRetreatHome(registry, rallyPos);
            RetreatIfLowHP(registry, map, &occ, home, 0, kRetreatHealthFraction, true);
        }
        if (!sandboxMode)
        {
            ai.Update(dt); // M8: enemy build order, waves, scouting, retreat
        }
        // 2v2 overflow commanders tick only in 2v2 matches (see worldIs2v2:
        // count-gating wakes parked commanders in 1v1 via shared teams).
        if (worldIs2v2)
        {
            allyAI.Update(dt);   // allied build order + waves beside the player
            enemyAI2.Update(dt); // second enemy front
        }
        // M6 Goal 1: periodic minimap refresh (terrain blocks + unit dots).
        if (minimap.PollRefresh(dt))
        {
            BeginTextureMode(minimap.target);
            ClearBackground(Color{ 20, 60, 20, 255 }); // dark grass base
            for (int y = 0; y < map.Height(); ++y)
            {
                for (int x = 0; x < map.Width(); ++x)
                {
                    const TerrainType terrain = map.Get({ x, y });
                    if (terrain == TerrainType::Grass)
                    {
                        continue;
                    }
                    const Vector2 corner = minimap.WorldToMinimap(cc::ToRaylib(cc::TileToWorld(x, y)),
                                                                 map.Width(), map.Height());
                    const Color tint = terrain == TerrainType::Water    ? DARKBLUE
                                     : terrain == TerrainType::Forest ? Color{ 20, 90, 20, 255 }
                                     : terrain == TerrainType::Rock   ? GRAY
                                                                      : DARKGRAY;
                    DrawRectangleV({ corner.x - minimap.screenRect.x, corner.y - minimap.screenRect.y },
                                   { 8.0f, 8.0f }, tint);
                }
            }
            registry.Each<Unit>([&](Entity, const Unit &unit) {
                // M9: enemies only appear when a team-0 unit sees their tile.
                if (unit.teamID != 0 &&
                    !fog.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit.position))))
                {
                    return;
                }
                const Vector2 center = { unit.position.x + 32.0f, unit.position.y + 32.0f };
                const Vector2 dot =
                    minimap.WorldToMinimap(center, map.Width(), map.Height());
                DrawRectangle(static_cast<int>(dot.x - minimap.screenRect.x) - 1,
                              static_cast<int>(dot.y - minimap.screenRect.y) - 1, 3, 3,
                              art.TeamTint(unit.teamID));
            });
            EndTextureMode();
        }
        // M6 Goal 3: decide terminal states from the living rosters.
        // Skipped in sandbox mode (no enemy side: instant Victory otherwise).
        if (!sandboxMode)
        {
            menu.ShowOutcome(TeamHasUnits(registry, 0), TeamHasUnits(registry, 1));
        }
        // M11: fanfare on the transition frame only.
        // M14: Q57 game-state events ride the same transition.
        if (menu.state != lastOutcomeState)
        {
            if (menu.state == MenuState::Victory)
            {
                audio.Play(SfxId::Victory);
                Announce(EventType::Victory);
            }
            else if (menu.state == MenuState::GameOver)
            {
                audio.Play(SfxId::Defeat);
                Announce(EventType::GameOver);
            }
            lastOutcomeState = menu.state;
        }
    }

    // M11: volumes follow the pause-menu sliders live; the loop streams on.
    audio.ApplySettings(menu.settings.masterVolume, menu.settings.musicVolume,
                        menu.settings.sfxVolume, menu.settings.mute);
    audio.UpdateMusic();

    // QoL replay viewer auto-advance (outside the Playing sim gate: the
    // viewer never simulates). Steps at the recording cadence; holds on
    // the last frame instead of wrapping.
    if (menu.state == MenuState::ReplayViewer && replayCount > 0)
    {
        replayPlayTimer += GetFrameTime();
        if (replayPlayTimer >= 2.0f)
        {
            replayPlayTimer = 0.0f;
            if (replayCursor + 1 < replayCount)
            {
                StepReplay(1);
            }
        }
    }
    pings.Update(GetFrameTime()); // QoL: UI clock — ages in menus/viewer too

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode2D(camera.view);

    // Tile grid: water/forest/rock filled, grass outlined.
    for (int y = 0; y < map.Height(); ++y)
    {
        for (int x = 0; x < map.Width(); ++x)
        {
            const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
            const TerrainType terrain = map.Get({ x, y });
            if (terrain == TerrainType::Water)
            {
                DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, SKYBLUE);
            }
            else if (terrain == TerrainType::Forest)
            {
                DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, DARKGREEN);
            }
            else if (terrain == TerrainType::Rock)
            {
                DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, GRAY);
            }
            DrawRectangleLinesEx({ corner.x, corner.y, cc::TILE_SIZE, cc::TILE_SIZE }, 1.0f, LIGHTGRAY);
        }
    }

    // M5: buildings as footprint rects with a type letter, nodes as
    // kind-colored discs with remaining amounts.
    registry.Each<Building>([&](Entity, const Building &building) {
        // M9: enemy structures hide until a team-0 unit sees a footprint tile.
        if (building.teamID != 0 &&
            !fog.IsVisible(0, cc::IVec2{ building.tileX, building.tileY }))
        {
            return;
        }
        const cc::IVec2 size = Footprint(building.type);
        const Vector2 corner = cc::ToRaylib(cc::TileToWorld(building.tileX, building.tileY));
        if (art.UseRectangles())
        {
            const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
            const float h = static_cast<float>(size.y) * cc::TILE_SIZE;
            const Color tint =
                building.type == BuildingType::Base ? DARKGRAY : building.type == BuildingType::Factory ? BROWN : GRAY;
            DrawRectangleV(corner, { w, h }, tint);
            const char *label =
                building.type == BuildingType::Base ? "B" : building.type == BuildingType::Factory ? "F" : "D";
            DrawText(label, static_cast<int>(corner.x) + 6, static_cast<int>(corner.y) + 4, 24, WHITE);
        }
        else
        {
            art.DrawBuilding(building.type, building.teamID, building.tileX, building.tileY);
        }
        if (building.isSelected)
        {
            // QoL building-selection highlight (mirrors the unit outline).
            const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
            const float h = static_cast<float>(size.y) * cc::TILE_SIZE;
            DrawRectangleLinesEx({ corner.x, corner.y, w, h }, 3.0f, RED);
        }
    });
    // QoL area-build ghost: footprint outline under the cursor, green when
    // CanPlaceBuilding passes, red when it doesn't. Stays engaged across
    // placements (right-click/Esc exits); 1/2/3 switches the type.
    if (placingType.has_value())
    {
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(input.MouseWorld(camera)));
        const cc::IVec2 fp = Footprint(*placingType);
        const Vector2 ghostCorner = cc::ToRaylib(cc::TileToWorld(tile.x, tile.y));
        const bool ok = CanPlaceBuilding(map, &nodes, *placingType, tile.x, tile.y);
        DrawRectangleLinesEx({ ghostCorner.x, ghostCorner.y,
                               static_cast<float>(fp.x) * cc::TILE_SIZE,
                               static_cast<float>(fp.y) * cc::TILE_SIZE },
                             2.0f, ok ? GREEN : RED);
        DrawText(TextFormat("Placing: %s (1/2/3 type, Z/Esc done)",
                            BuildingTypeName(*placingType)),
                 static_cast<int>(ghostCorner.x), static_cast<int>(ghostCorner.y) - 20, 14,
                 ok ? DARKGREEN : RED);
    }
    nodes.Each([&](const ResourceNode &node) {
        // M9: static features join the frozen snapshot once explored.
        if (!fog.IsExplored(0, node.tile))
        {
            return;
        }
        const Vector2 corner = cc::ToRaylib(cc::TileToWorld(node.tile.x, node.tile.y));
        const Vector2 center = { corner.x + cc::TILE_SIZE / 2.0f,
                                 corner.y + cc::TILE_SIZE / 2.0f };
        if (art.UseRectangles())
        {
            DrawCircleV(center, 14.0f, node.kind == ResourceKind::Iron ? GOLD : LIME);
        }
        else
        {
            art.DrawNode(node.kind, center);
        }
        DrawText(TextFormat("%.0f", node.amount), static_cast<int>(center.x) - 12,
                 static_cast<int>(center.y) - 8, 12, DARKGRAY);
    });

    // Units as sprites (M12) or 32x32 placeholder rects (fallback/tests):
    // red-ringed when selected, tracer to the target when Attacking (M3G5),
    // white-flashed with a damage number while hitFlashTime runs (M4G5).
    // Selected units also get a health bar (M6 Goal 4).
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        // M9: enemies render only on tiles team 0 currently sees.
        if (unit.teamID != 0 &&
            !fog.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit.position))))
        {
            return;
        }
        const Rectangle body = { unit.position.x + 16.0f, unit.position.y + 16.0f, 32.0f, 32.0f };
        const Vector2 center = { body.x + 16.0f, body.y + 16.0f };
        if (art.UseRectangles() && !art.UseAtlas())
        {
            // Team as the base color (previously attack-state only, which
            // made teams indistinguishable in this tier); attacking keeps
            // a separate outline cue so both signals survive.
            DrawRectangleRec(body, art.TeamTint(unit.teamID));
            if (unit.state == UnitState::Attacking)
            {
                DrawRectangleLinesEx(body, 2.0f, ORANGE);
            }
        }
        else
        {
            // Phase 14: infantry draws as a squad cluster; vehicles draw
            // as a single sprite (count == 1, offset {0,0}). Atlas-first:
            // moving units cycle "<type>_walk", everything else idles on
            // "<type>_idle_0_0"; types without atlas entries fall through
            // to the legacy DrawUnit (or a tinted rect when that is down).
            std::array<Vector2, 6> slots;
            float slotScale = 1.0f;
            const int count = SquadSlots(unit.type, id, UnitHealthFraction(unit), slots, slotScale);
            const bool moving = unit.state == UnitState::Moving;
            const float animTime = static_cast<float>(GetTime());
            const Color tint = art.TeamTint(unit.teamID);
            for (int i = 0; i < count; ++i)
            {
                const Vector2 corner = { unit.position.x + slots[i].x,
                                         unit.position.y + slots[i].y };
                if (art.UseAtlas())
                {
                    const std::string sprite = art.UnitSprite(unit.type, moving, id, animTime);
                    if (!sprite.empty())
                    {
                        art.DrawAtlasFrame(sprite, corner, tint, slotScale);
                        continue;
                    }
                }
                if (!art.UseRectangles())
                {
                    art.DrawUnit(unit.type, unit.teamID, FrameForPhase(unit.phase), corner);
                }
                else
                {
                    DrawRectangleRec({ corner.x + 26.0f, corner.y + 26.0f, 12.0f, 12.0f },
                                     tint);
                }
            }
        }
        if (unit.isSelected)
        {
            DrawRectangleLinesEx(body, 3.0f, RED);
            const float fraction = UnitHealthFraction(unit);
            DrawRectangle(static_cast<int>(body.x), static_cast<int>(body.y) - 8,
                          static_cast<int>(body.width), 5, Fade(RED, 0.6f));
            DrawRectangle(static_cast<int>(body.x), static_cast<int>(body.y) - 8,
                          static_cast<int>(body.width * fraction), 5, GREEN);
            // QoL range preview: attack-radius ring for selected units.
            if (unit.attackRange > 0)
            {
                DrawCircleLinesV(center, static_cast<float>(unit.attackRange),
                                 Fade(RED, 0.35f));
            }
        }
        if (unit.controlGroups != 0)
        {
            // QoL control-group badge: lowest group number, above the bar.
            int lowestBit = 0;
            while (lowestBit < 9 && (unit.controlGroups & (1u << lowestBit)) == 0)
            {
                ++lowestBit;
            }
            DrawText(TextFormat("%d", (lowestBit + 1) % 10), static_cast<int>(body.x),
                     static_cast<int>(body.y) - 22, 12, DARKBLUE);
        }
        if (unit.hitFlashTime > 0.0f)
        {
            DrawRectangleRec(body, Fade(WHITE, 0.7f));
            DrawText(TextFormat("-%.0f", unit.lastDamageTaken), static_cast<int>(body.x),
                     static_cast<int>(body.y) - 18, 16, RED);
        }
        if (unit.state == UnitState::Attacking)
        {
            if (const Unit *target = registry.Get<Unit>(unit.target))
            {
                const Vector2 targetCenter = { target->position.x + 32.0f, target->position.y + 32.0f };
                DrawLineV(center, targetCenter, RED);
            }
        }
        if (unit.hasMoveOrder)
        {
            DrawCircleV(cc::ToRaylib(cc::ToGlm(unit.moveTarget) + cc::Vec2(32.0f, 32.0f)), 5.0f, GREEN);
        }
    });
    // M12: particles in world space, under the shroud so hidden battles
    // stay hidden.
    art.ParticlesPool().Draw();
    // M9 shroud, drawn over the world: unexplored tiles go opaque black,
    // explored-but-unseen tiles get a dim veil (frozen snapshot, Q76).
    for (int y = 0; y < map.Height(); ++y)
    {
        for (int x = 0; x < map.Width(); ++x)
        {
            const cc::IVec2 tile{ x, y };
            if (fog.IsVisible(0, tile))
            {
                continue;
            }
            const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
            const Color veil =
                fog.IsExplored(0, tile) ? Fade(BLACK, 0.45f) : BLACK;
            DrawRectangleV(corner, { cc::TILE_SIZE, cc::TILE_SIZE }, veil);
        }
    }
    EndMode2D();

    // Drag-box visual (screen space, under the HUD panels).
    if (dragging && input.LeftDown())
    {
        DrawRectangleLinesEx(NormalizeRect(dragStart, input.MouseScreen()), 1.0f, GREEN);
    }
    // QoL area-repair preview (screen space, same layer as drag-box):
    // live zone rect plus a marker on every damaged candidate that would
    // be assigned on release.
    if (repairDragActive && input.LeftDown() && areaRepairMode)
    {
        DrawRectangleLinesEx(NormalizeRect(repairDragStart, input.MouseScreen()), 1.0f,
                             GREEN);
        std::vector<Entity> preview;
        CollectAreaRepairCandidates(
            registry, DraggedWorldBox(camera, repairDragStart, input.MouseScreen()), 0,
            preview);
        for (const Entity id : preview)
        {
            Vector2 center = { 0.0f, 0.0f };
            if (const Unit *unit = registry.Get<Unit>(id))
            {
                center = { unit->position.x + 32.0f, unit->position.y + 32.0f };
            }
            else if (const Building *building = registry.Get<Building>(id))
            {
                const Rectangle fp = BuildingFootprintRect(*building);
                center = { fp.x + fp.width / 2.0f, fp.y + fp.height / 2.0f };
            }
            DrawCircleV(GetWorldToScreen2D(center, camera.view), 4.0f, GREEN);
        }
    }
    // QoL line-formation preview (screen space, same layer as drag-box).
    if (rightDragging && input.RightDown())
    {
        DrawLineEx(rightDragStart, input.MouseScreen(), 2.0f, SKYBLUE);
        DrawCircleV(rightDragStart, 3.0f, SKYBLUE);
        DrawCircleV(input.MouseScreen(), 3.0f, SKYBLUE);
    }

    // M6 Goal 1: minimap blit (texture is Y-flipped) + viewport box.
    // Hidden from the settings panel (M6 Goal 3).
    if (menu.settings.showMinimap)
    {
        DrawTextureRec(minimap.target.texture,
                       { 0.0f, 0.0f, minimap.screenRect.width, -minimap.screenRect.height },
                       { minimap.screenRect.x, minimap.screenRect.y }, WHITE);
        DrawRectangleLinesEx(
            minimap.ViewportRect(camera.view, screenWidth, screenHeight, map.Width(), map.Height()),
            1.0f, WHITE);
        DrawRectangleLinesEx(minimap.screenRect, 1.0f, DARKGRAY);
        // QoL ping blips: pulsing markers, fading by age. Red = under
        // attack, orange = unit lost.
        for (const Ping &ping : pings.Active())
        {
            const Vector2 blip = minimap.WorldToMinimap(ping.worldPos, map.Width(), map.Height());
            const float pulse = 2.0f + ping.age * 2.0f;
            const Color color = ping.kind == PingKind::UnderAttack
                                    ? Fade(RED, 1.0f - ping.age / 5.0f)
                                    : Fade(ORANGE, 1.0f - ping.age / 5.0f);
            DrawCircleV(blip, pulse, color);
        }
    }

    // M6 Goal 2: raygui HUD (proper panels replace the M5 text counters).
    DrawResourcePanel(resources, &art);
    DrawSelectionPanel(registry);
    DrawIdleButtons(registry, 0); // QoL: team 0 is the player
    DrawRepairPanel(&playerAutoRepair, &autoRepairCap);
    DrawControlGroupStrip(registry, 0, autoAddGroupBit); // QoL: team 0 is the player
    DrawSaveSlots();
    // M13: factory panel (build buttons, queue, cancel); rally hint
    // while placing the rally point. Recomputed here (not just the sim
    // gate above) so the panel stays correct while paused.
    registry.Each<Building>([&](Entity, const Building &building) {
        if (building.teamID == 0 && building.type == BuildingType::Factory &&
            building.state == BuildingState::Operational)
        {
            hasFactory = true;
        }
    });
    DrawProductionPanel(resources, queue, hasFactory);
    if (settingRally)
    {
        DrawText("Rally: left-click to place (R cancels)", 250, 364, 16, DARKGREEN);
    }
    if (attackGroundMode)
    {
        DrawText("Shelling: right-click to fire (X/Esc cancels)", 250, 364, 16, RED);
    }
    if (!queue.Empty())
    {
        const float ppw = 174.0f;
        const float ppx = static_cast<float>(screenWidth) - ppw - 10.0f;
        DrawText("Producing...", static_cast<int>(ppx), screenHeight - 222, 16, GRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, 150, 12, LIGHTGRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, static_cast<int>(150.0f * queue.HeadProgress()), 12, DARKGREEN);
    }

    // M7 Goal 3: live frame-rate readout (60 FPS target validation).
    DrawFPS(screenWidth - 170, 135);
    // M8: enemy commander status (M14: difficulty comes from the setup).
    DrawText(TextFormat("Enemy: %s  Waves: %d", DifficultyName(worldDifficulty),
                        ai.WavesLaunched()),
             screenWidth - 170, 155, 16, GRAY);

    // M6 Goal 4: shortcut overlay, bottom-left, toggled with F1.
    if (showHints)
    {
        const std::vector<std::string> hints = ShortcutHintLines();
        for (std::size_t i = 0; i < hints.size(); ++i)
        {
            DrawText(hints[i].c_str(), 8, 250 + static_cast<int>(i) * 18, 14, Fade(DARKGRAY, 0.8f));
        }
    }

    // M6 Goal 3: menu overlays sit on top of the frame.
    if (menu.state == MenuState::HotkeyRemap)
    {
        // Entered from pause (world stays frozen: the sim only advances in
        // Playing). Same screen as the Settings-chain branch above.
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        DrawHotkeyRemap(screenWidth / 2.0f);
    }
    if (menu.state == MenuState::ReplayViewer)
    {
        // QoL replay banner (no window: the world render stays visible).
        // Snapshot slideshow, not a re-simulation — production queues were
        // never saved, so units appear at snapshot boundaries.
        DrawText(TextFormat("Replay %d/%d", replayCursor + 1, replayCount), 8, 96, 16,
                 DARKGRAY);
        DrawText("Left/Right step - Esc exit", 8, 116, 14, Fade(DARKGRAY, 0.8f));
    }
    if (menu.state == MenuState::Paused)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        if (GuiWindowBox(Rectangle{ 250, 40, 300, 400 }, "Paused"))
        {
            menu.state = MenuState::Playing;
        }
        if (GuiButton(Rectangle{ 270, 85, 260, 30 }, "Resume"))
        {
            Announce(EventType::MenuAction);
            menu.state = MenuState::Playing;
        }
        GuiLabel(Rectangle{ 270, 122, 260, 20 }, "Camera speed");
        GuiSlider(Rectangle{ 270, 145, 260, 20 }, "100", "800", &menu.settings.cameraSpeed,
                  100.0f, 800.0f);
        GuiCheckBox(Rectangle{ 270, 170, 20, 20 }, "Minimap", &menu.settings.showMinimap);
        // M11: volumes (0..1) + mute, persisted by the M14 settings file.
        GuiLabel(Rectangle{ 270, 195, 260, 20 }, "Master volume");
        GuiSlider(Rectangle{ 270, 218, 260, 20 }, "0", "1", &menu.settings.masterVolume,
                  0.0f, 1.0f);
        GuiLabel(Rectangle{ 270, 243, 260, 20 }, "Music volume");
        GuiSlider(Rectangle{ 270, 266, 260, 20 }, "0", "1", &menu.settings.musicVolume,
                  0.0f, 1.0f);
        GuiLabel(Rectangle{ 270, 291, 260, 20 }, "SFX volume");
        GuiSlider(Rectangle{ 270, 314, 260, 20 }, "0", "1", &menu.settings.sfxVolume,
                  0.0f, 1.0f);
        GuiCheckBox(Rectangle{ 270, 340, 20, 20 }, "Mute", &menu.settings.mute);
        GuiCheckBox(Rectangle{ 270, 365, 20, 20 }, "Right-drag pan",
                    &menu.settings.rightDragPan);
        GuiCheckBox(Rectangle{ 270, 390, 20, 20 }, "Color-blind mode",
                    &menu.settings.colorBlindMode);
        art.SetColorBlindMode(menu.settings.colorBlindMode); // live, no reopen needed
        if (GuiButton(Rectangle{ 270, 415, 260, 30 }, "Remap hotkeys..."))
        {
            remapArming = -1;
            remapConflictAction.clear();
            remapReturn = MenuState::Paused;
            Announce(EventType::MenuAction);
            menu.state = MenuState::HotkeyRemap;
        }
        // M14: pause shares MenuSettings with the settings screen; leaving
        // via Back-equivalent persists (Q86), pause buttons apply live.
        if (GuiButton(Rectangle{ 270, 450, 260, 30 }, "Quit to menu"))
        {
            QuitToMenu();
        }
        if (GuiButton(Rectangle{ 270, 485, 260, 30 }, "Quit to desktop"))
        {
            menu.quitRequested = true;
        }
    }
    else if (menu.state == MenuState::GameOver || menu.state == MenuState::Victory)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
        const bool won = menu.state == MenuState::Victory;
        if (GuiWindowBox(Rectangle{ 250, 140, 300, 195 }, won ? "Victory!" : "Defeat"))
        {
            menu.quitRequested = true;
        }
        GuiLabel(Rectangle{ 270, 185, 260, 20 },
                 won ? "Enemy force destroyed." : "Your force was destroyed.");
        if (GuiButton(Rectangle{ 270, 240, 260, 30 }, "Return to menu"))
        {
            QuitToMenu();
        }
        if (GuiButton(Rectangle{ 270, 280, 260, 30 }, "Quit to desktop"))
        {
            menu.quitRequested = true;
        }
    }
    EndDrawing();
}

void Game::Shutdown()
{
    minimap.Unload();
    art.Shutdown(); // M12: unload sprite textures
    audio.Shutdown(); // M11: unload sounds + music stream
    CloseAudioDevice();
    CloseWindow();
}

bool Game::IsRunning() const
{
    return !WindowShouldClose() && !menu.quitRequested;
}

int Game::Run()
{
    Init();
    while (IsRunning())
    {
        Update();
    }
    Shutdown();
    return 0;
}
