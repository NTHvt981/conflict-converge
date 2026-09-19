// Game.cpp - owns the former main.cpp boot state + frame loop.
// main.cpp is only the entry point (construct, Run, return).

#include "Game.h"

#include "raygui.h"
#include "Building.h"
#include "Cursor.h"
#include "DataRoot.h"
#include "Formation.h"
#include "Hud.h"
#include "Log.h"
#include "MapFile.h"
#include "Pathfinder.h"
#include "Selection.h"
#include "Shortcuts.h"
#include "Unit.h"
#include <algorithm>
#include <filesystem>
#include <vector>
#include <cmath>

namespace
{
// Selection outline box: union of the rendered sprite rects. Atlas
// clusters (and 2x prototype art) spread past the fixed 32x32 body inset,
// so the red border follows the sprites instead of slicing through them.
// Per-slot geometry mirrors the render branch exactly: atlas cells
// origin-align to tileCorner+32 (the origin lands on the tile center),
// flat PNGs blit 32x32 at corner+16. Rectangle tier (no per-sprite
// geometry) keeps the body. Flat-PNG singles reproduce the old body box
// exactly; atlas singles hug their narrower cell (intended: the border
// follows sprites, not the inset).
Rectangle SquadSelectionBox(const Art &art, const Unit &unit, Entity id, bool atlasPath,
                            Rectangle body)
{
    if (!atlasPath)
    {
        return body;
    }
    std::array<Vector2, 6> slots;
    float slotScale = 1.0f;
    const int count = SquadSlots(unit.type, id, UnitHealthFraction(unit), slots, slotScale);
    // Only the infantry sheets live in the atlas today (16x32 cells);
    // everything else clusters (AntiArmor) or singles (vehicles) from
    // 32x32 flat PNGs. Revisit if atlas art diversifies per type.
    const bool atlasCells =
        art.UseAtlas() && !art.UnitSprite(unit.type, false, id, 0.0f, 0).empty();
    const float s = Art::BaseArtScale(unit.type);
    float x0, y0, x1, y1;
    if (atlasCells)
    {
        x0 = unit.position.x + slots[0].x + 32.0f - 8.0f * s;
        y0 = unit.position.y + slots[0].y + 32.0f - 16.0f * s;
        x1 = unit.position.x + slots[0].x + 32.0f + 8.0f * s;
        y1 = unit.position.y + slots[0].y + 32.0f + 16.0f * s;
    }
    else
    {
        x0 = unit.position.x + slots[0].x + 16.0f;
        y0 = unit.position.y + slots[0].y + 16.0f;
        x1 = x0 + 32.0f;
        y1 = y0 + 32.0f;
    }
    for (int i = 1; i < count; ++i)
    {
        float cx0, cy0, cx1, cy1;
        if (atlasCells)
        {
            cx0 = unit.position.x + slots[i].x + 32.0f - 8.0f * s;
            cy0 = unit.position.y + slots[i].y + 32.0f - 16.0f * s;
            cx1 = unit.position.x + slots[i].x + 32.0f + 8.0f * s;
            cy1 = unit.position.y + slots[i].y + 32.0f + 16.0f * s;
        }
        else
        {
            cx0 = unit.position.x + slots[i].x + 16.0f;
            cy0 = unit.position.y + slots[i].y + 16.0f;
            cx1 = cx0 + 32.0f;
            cy1 = cy0 + 32.0f;
        }
        x0 = std::min(x0, cx0);
        y0 = std::min(y0, cy0);
        x1 = std::max(x1, cx1);
        y1 = std::max(y1, cy1);
    }
    return { x0 - 2.0f, y0 - 2.0f, (x1 - x0) + 4.0f, (y1 - y0) + 4.0f };
}
// Setup-screen and HUD difficulty label.
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
    // Enemy commander owns team 1 under fair rules. Parked without a
    // base until the first Start/Load re-arms it (BuildSkirmish runs Reset +
    // SetupBase; the load path runs Reset bare since the file fields the AI).
    , ai(registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , allyAI(registry, map, nodes, events, 0, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , enemyAI2(registry, map, nodes, events, 1, AIDifficulty::Medium, { 0, 0 }, { 0, 0 })
    , skirmish{ &registry, &resources, &map, &occ, &fog, &nodes,
                &queue,   &factory,   &ai, &allyAI, &enemyAI2, &camera, &rallyPos }
    // Shared snapshot for the save-slot bindings.
    , worldState{ &registry, &resources, &map, &camera, &nodes, &fog, &occ }
    // Input dispatch (binds world + input members above).
    , playingInput(registry, map, occ, nodes, camera, minimap, input, audio, menu.settings,
                   rallyPos)
    // Match tick (binds the members above; declared last for the same reason).
    , sim(registry, map, occ, fog, nodes, queue, factory, resources, ai, allyAI, enemyAI2,
          art, audio, pings, menu, minimap, worldState, damageNumbers, events, rallyPos,
          playingInput.AutoAddGroupBit(), sandboxMode, worldIs2v2, playerAutoRepair,
          autoRepairCap, shakeTrauma, lastOutcomeState)
    // Menu screens (world transitions stay here as callbacks).
    , menuScreens(menu, art, audio, input, hotkeys, events,
                  MenuCallbacks{
                      [&](const std::string &mapPath, AIDifficulty difficulty) {
                          StartMatch(mapPath, difficulty);
                      },
                      [&](const std::string &slotPath) { LoadGameFromSlot(slotPath); },
                      [&]() { WatchLastReplay(); },
                      [&]() { BindShortcuts(); },
                  })
{
}

void Game::Init()
{
    // Launch hardening first (windowless-safe: pure filesystem + module
    // path): a foreign CWD breaks every data/ path below, so chdir to the
    // data root when CWD doesn't resolve it. No-op for F5 + double-click.
    if (const char *dataRoot =
            PickDataRoot(DirectoryExists("data"), GetApplicationDirectory(), DirectoryExists))
    {
        ChangeDirectory(dataRoot);
    }

    // Fatal log file from here on (console + data/logs/conflict-converge.log);
    // Init falls back to console-only when the file cannot be opened.
    Log::Init("data/logs/conflict-converge.log");

    const int kInitialWidth = 1200;
    const int kInitialHeight = 675;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE); // User-resizable window
    InitWindow(kInitialWidth, kInitialHeight, "raylib basic window");
    SetWindowMinSize(800, 450); // HUD layout assumes at least this
    SetTargetFPS(60);
    GuiEnableTooltip(); // hover tooltips on HUD/menu controls (GuiSetTooltip sites)

    // Audio device + asset load. The test binary never inits (headless).
    InitAudioDevice();
    audio.Init(IsAudioDeviceReady());
    // Sprite + particle renderer. Missing files fall back to the
    // rectangle placeholders the tests exercise headless.
    art.Init(true);
    if (art.HasFont())
    {
        GuiSetFont(art.UiFont()); // raygui controls render in Porto Buena
    }
    // Outcome-transition edge (sim edge-trigger polls live in Simulation).
    lastOutcomeState = MenuState::MainMenu; // Boot to title

    camera.view.offset = { kInitialWidth / 2.0f, kInitialHeight / 2.0f };
    camera.view.rotation = 0.0f;
    camera.view.zoom = 1.0f;
    // Menu flow (pause/outcome/settings); camera speed is a
    // live setting, not a constant, so the settings slider can tune it.
    // Boots at MainMenu; persisted settings load here, ignored
    // when the file is missing.
    LoadSettings(menu.settings, kSettingsPath);
    art.SetColorBlindMode(menu.settings.colorBlindMode); // QoL: persisted palette
    GuiSetStyle(DEFAULT, TEXT_SIZE, static_cast<int>(10 * menu.settings.uiScale)); // persisted UI scale
    menuScreens.ApplyHotkeyOverrides(); // QoL: persisted remaps, before BindShortcuts below

    // Minimap texture (top-right, 4:3 like the 20x15 map).
    minimap.Init({ static_cast<float>(kInitialWidth) - 170.0f, 10.0f,
                   160.0f, 120.0f });
    // Unit speed now comes from base stats (Unit::speed, ApplyBaseStats).

    // World objects are boot-level state, but match CONTENT only builds
    // after Start confirms (BuildSkirmish) or a slot loads. Build/Reset refill
    // contents in place, so shortcut lambdas plus the factory/AI reference
    // bindings stay valid across matches.
    // Match session — nothing simulates or renders until Start.
    worldActive = false;
    worldIs2v2 = false;
    worldDifficulty = AIDifficulty::Medium;
    worldMapPath.clear(); // map the active match was seeded from
    showHints = true; // F1 toggles the shortcut overlay

    // Production/spawn confirmations (stateless: survives across matches).
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
    // Bare-event announcer for the game-state + UI event types.
    Event bare;
    bare.type = type;
    events.Dispatch(bare);
}

// (Re)start a skirmish from the setup screen: seed the world, reset
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
    playingInput.ResetForMatch(); // clear transient gestures for the fresh match
    lastOutcomeState = MenuState::Playing;
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval; // repaint for the new map now
    // QoL snapshot replay: fresh recording for this match (last match only;
    // the directory is cleared and frames renumbered from zero).
    sim.ResetForMatch(); // edge polls + factory gate + recording (viewer cursor stays here)
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

// Teardown back to the title (world contents dropped).
void Game::QuitToMenu()
{
    ResetSkirmish(skirmish);
    worldActive = false;
    worldIs2v2 = false;
    sandboxMode = false;
    sim.StopRecording(); // QoL: keep this match's frames for the viewer
    Announce(EventType::MenuAction);
    menu.OpenMainMenu();
}

// Slot-load apply for the menu load screen: world rebuild over a fresh
// shell plus commander re-arm. The slot UI lives in MenuScreens.
void Game::LoadGameFromSlot(const std::string &slotPath)
{
    // Load over a fresh shell (LoadWorld clears +
    // rebuilds). The file already fields the AI side, so
    // the commander re-arms bare — no second SetupBase.
    ResetSkirmish(skirmish);
    if (!LoadWorld(worldState, slotPath))
    {
        return;
    }
    const SkirmishSpots spots =
        SpotsForMap(worldMapPath.empty() ? "data/maps/crossroads.map" : worldMapPath);
    ai.Reset(menu.setup.difficulty, spots.aiHome, spots.playerHome, 1);
    if (spots.is2v2)
    {
        // 2v2 save: the file already fields all three AI
        // sides, so all commanders re-arm bare.
        allyAI.Reset(menu.setup.difficulty, spots.allyHome, spots.aiHome, 0);
        enemyAI2.Reset(menu.setup.difficulty, spots.enemyHome2, spots.playerHome, 1);
    }
    rallyPos = camera.view.target;
    worldDifficulty = menu.setup.difficulty;
    worldActive = true;
    worldIs2v2 = spots.is2v2;
    playingInput.ResetForMatch(); // clear transient gestures for the loaded world
    sim.ResetEdgePolls(); // fresh polls for the loaded world (no recording)
    lastOutcomeState = MenuState::Playing;
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval;
    menu.state = MenuState::Playing;
    Announce(EventType::MenuAction);
    Announce(EventType::MatchStarted);
}

void Game::StepReplay(int dir)
{
    if (sim.ReplayCount() <= 0)
    {
        return;
    }
    replayCursor += dir;
    if (replayCursor < 0)
    {
        replayCursor = 0;
    }
    if (replayCursor >= sim.ReplayCount())
    {
        replayCursor = sim.ReplayCount() - 1;
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
    sim.StopRecording();
    sim.SetReplayCount(count);
    replayCursor = 0;
    replayPlayTimer = 0.0f;
    worldActive = true;
    worldIs2v2 = false;
    playingInput.ResetForMatch(); // viewer never inherits match gestures
    camera.view.zoom = 1.0f;
    minimap.elapsed = minimap.refreshInterval;
    menu.state = MenuState::ReplayViewer;
    Announce(EventType::MenuAction);
    return true;
}

void Game::BindShortcuts()
{
    // Idempotent rebuild: Clear first so rebound-away keys leave no stale
    // binding behind (Bind overwrites same-key slots but never clears the
    // old key). Every literal goes through hotkeys.KeyFor — the remap
    // screen + settings file own the effective keys, not these defaults.
    input.shortcuts.Clear();
    // Shortcuts, pumped by the InputManager: Esc
    // deselects, Space halts selected units, P pauses,
    // F1 toggles the shortcut overlay.
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
    // Named save slots (F6-8 store, Shift+F6-8 recall).
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
    // Order keys act on the current selection. A attack-moves to the
    // cursor, H/G switch stances, V patrols cursor-and-back, R toggles
    // rally-point placement. : world keys are dead outside a live match.
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
            playingInput.ToggleSettingRally();
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
            playingInput.ToggleSlowestSpeed();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AreaBuild"), [&] {
        // QoL area-build placement flow: toggle, defaulting to Base.
        // While engaged, 1/2/3 picks the type (see the group loop guard),
        // left-click/drag places, right-click or Esc cancels.
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleAreaBuild();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AreaRepair"), [&] {
        // QoL area-repair mode: toggle, then left-drag a repair zone
        // (mirrors area-build's toggle-then-drag shape).
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleAreaRepair();
        }
    });
    input.shortcuts.Bind(hotkeys.KeyFor("AttackGround"), [&] {
        // QoL attack-ground mode: arm shelling; the next right-click fires
        // it (one-shot, see the RightPressed block), Escape cancels.
        if (worldActive && menu.state == MenuState::Playing)
        {
            playingInput.ToggleAttackGround();
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
        // Esc backs out of menu screens; in-match it keeps the
        // deselect behavior (Paused resumes).
        if (menuScreens.CancelRemapCapture())
        {
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
            playingInput.CancelForEsc();
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
                unit.target = kInvalidEntity; // Halt drops combat too
                unit.attackMove = false; // Halt drops attack-move + repair too
                unit.hasRepairOrder = false;
                unit.repairTarget = kInvalidEntity;
                unit.hasAttackGroundOrder = false; // QoL: halt drops shelling too
                unit.speedCapPixelsPerSec = -1.0f; // QoL: halt drops the group cap
                unit.orderQueue.clear(); // QoL: halt drops queued orders too
                unit.phase = AttackPhase::Ready; // Halt cancels the telegraph
                unit.velocity = { 0.0f, 0.0f };
                unit.state = UnitState::Idle;
                SnapUnitToTile(unit);
            }
        });
    });
}

// World render (H6 slice 3a of Update): cursor intent, shaken camera,
// tile grid, buildings, placement ghost, nodes, units, particles,
// shroud. Runs inside the caller's Begin/EndDrawing pair.
void Game::DrawWorld()
{
    // Context cursors: predict right-click intent once per frame from live
    // state (placement validity, attack-ground mode, hovered unit) and push
    // the matching system cursor. Runs before BeginMode2D so the frame's
    // input handling is fully settled.
    {
        CursorIntent intent = CursorIntent::Default;
        if (playingInput.PlacingType().has_value())
        {
            const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(input.MouseWorld(camera)));
            intent = CanPlaceBuilding(map, &nodes, *playingInput.PlacingType(), tile.x, tile.y)
                         ? CursorIntent::Default
                         : CursorIntent::InvalidPlacement;
        }
        else if (playingInput.AttackGroundMode())
        {
            intent = CursorIntent::Attack; // next right-click shells the point
        }
        else
        {
            const Entity selectedId = SelectedUnit(registry);
            const Unit *selected =
                selectedId != kInvalidEntity ? registry.Get<Unit>(selectedId) : nullptr;
            intent = PredictCursorIntent(registry, selected, input.MouseWorld(camera));
        }
        switch (intent)
        {
        case CursorIntent::Attack:
            SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
            break;
        case CursorIntent::Repair:
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
            break;
        case CursorIntent::InvalidPlacement:
            SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED);
            break;
        case CursorIntent::Move:
            SetMouseCursor(MOUSE_CURSOR_ARROW);
            break;
        case CursorIntent::Default:
        default:
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            break;
        }
    }

    // Screen shake: jitter a COPY of the camera. camera.view itself stays
    // untouched so ClampToMap/Pan and the post-EndMode2D HUD math (world-
    // to-screen, minimap viewport) keep seeing the real camera.
    Camera2D shaken = camera.view;
    if (shakeTrauma > 0.0f)
    {
        const float amount = ShakeMagnitude(shakeTrauma);
        shaken.offset.x += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
        shaken.offset.y += static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f * amount;
    }
    BeginMode2D(shaken);

    // Tile grid: textured blits when art is up, flat fills on fallback.
    // (Building tiles draw no terrain either way — structure footprints
    // cover them, so there is no building tile to wire.)
    for (int y = 0; y < map.Height(); ++y)
    {
        for (int x = 0; x < map.Width(); ++x)
        {
            const Vector2 corner = cc::ToRaylib(cc::TileToWorld(x, y));
            const TerrainType terrain = map.Get({ x, y });
            if (art.UseRectangles())
            {
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
                // Fallback only: flat fills (and fill-less grass) need the
                // grid to read as tiles. Textured tiles cover their full
                // 64px, so lines would just slice the art.
                DrawRectangleLinesEx({ corner.x, corner.y, cc::TILE_SIZE, cc::TILE_SIZE }, 1.0f,
                                     LIGHTGRAY);
            }
            else
            {
                art.DrawTerrain(terrain, corner);
            }
        }
    }

    // Buildings as footprint rects with a type letter, nodes as
    // kind-colored discs with remaining amounts.
    registry.Each<Building>([&](Entity, const Building &building) {
        // Enemy structures hide until a team-0 unit sees a footprint tile.
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
            Art::DrawUiText(&art, label, static_cast<int>(corner.x) + 6, static_cast<int>(corner.y) + 4, 24, WHITE);
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
        if (building.state == BuildingState::UnderConstruction)
        {
            // World-space progress bar above the footprint (same two-rect
            // technique as the unit health bar below).
            const float w = static_cast<float>(size.x) * cc::TILE_SIZE;
            const float fraction = building.constructionTime / BuildingBuildTime(building.type);
            const float clamped = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
            DrawRectangle(static_cast<int>(corner.x), static_cast<int>(corner.y) - 10,
                          static_cast<int>(w), 6, LIGHTGRAY);
            DrawRectangle(static_cast<int>(corner.x), static_cast<int>(corner.y) - 10,
                          static_cast<int>(w * clamped), 6, DARKGREEN);
        }
    });
    // QoL area-build ghost: footprint outline under the cursor, green when
    // CanPlaceBuilding passes, red when it doesn't. Stays engaged across
    // placements (right-click/Esc exits); 1/2/3 switches the type.
    if (playingInput.PlacingType().has_value())
    {
        const cc::IVec2 tile = cc::WorldToTile(cc::ToGlm(input.MouseWorld(camera)));
        const cc::IVec2 fp = Footprint(*playingInput.PlacingType());
        const Vector2 ghostCorner = cc::ToRaylib(cc::TileToWorld(tile.x, tile.y));
        const bool ok = CanPlaceBuilding(map, &nodes, *playingInput.PlacingType(), tile.x, tile.y);
        DrawRectangleLinesEx({ ghostCorner.x, ghostCorner.y,
                               static_cast<float>(fp.x) * cc::TILE_SIZE,
                               static_cast<float>(fp.y) * cc::TILE_SIZE },
                             2.0f, ok ? GREEN : RED);
        Art::DrawUiText(&art, TextFormat("Placing: %s (1/2/3 type, Z/Esc done)",
                            BuildingTypeName(*playingInput.PlacingType())),
                 static_cast<int>(ghostCorner.x), static_cast<int>(ghostCorner.y) - 20, 14,
                 ok ? DARKGREEN : RED);
    }
    nodes.Each([&](const ResourceNode &node) {
        // Static features join the frozen snapshot once explored.
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
        Art::DrawUiText(&art, TextFormat("%.0f", node.amount), static_cast<int>(center.x) - 12,
                 static_cast<int>(center.y) - 8, 12, DARKGRAY);
    });

    // Units as sprites or 32x32 placeholder rects (fallback/tests):
    // red-ringed when selected, tracer to the target when Attacking,
    // white-flashed with a damage number while hitFlashTime runs.
    // Selected units also get a health bar.
    registry.Each<Unit>([&](Entity id, Unit &unit) {
        // Enemies render only on tiles team 0 currently sees.
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
            // Infantry draws as a squad cluster; vehicles draw
            // as a single sprite (count == 1, offset {0,0}). Atlas-first:
            // moving units cycle "<type>_walk", everything else idles on
            // "<type>_idle_0_0"; types without atlas entries fall through
            // to the legacy DrawUnit (or a tinted rect when that is down).
            std::array<Vector2, 6> slots;
            float slotScale = 1.0f; // cluster layout only; render scale is BaseArtScale
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
                    const std::string sprite = art.UnitSprite(unit.type, moving, id, animTime,
                                                            static_cast<int>(unit.facing));
                    if (!sprite.empty())
                    {
                        art.DrawAtlasFrame(sprite, corner, tint,
                                           Art::BaseArtScale(unit.type));
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
            // Border follows the rendered sprites (clusters/2x art spread
            // past the body); the bar floats above the border with a gap
            // instead of glued to the body top, where tall sprites touched it.
            const Rectangle selBox = SquadSelectionBox(
                art, unit, id, !(art.UseRectangles() && !art.UseAtlas()), body);
            DrawRectangleLinesEx(selBox, 3.0f, RED);
            const float fraction = UnitHealthFraction(unit);
            const float barY = selBox.y - 7.0f;
            DrawRectangle(static_cast<int>(selBox.x), static_cast<int>(barY),
                          static_cast<int>(selBox.width), 5, Fade(RED, 0.6f));
            DrawRectangle(static_cast<int>(selBox.x), static_cast<int>(barY),
                          static_cast<int>(selBox.width * fraction), 5, GREEN);
            // QoL range preview: attack-radius ring for selected units.
            if (unit.attackRange > 0)
            {
                DrawCircleLinesV(center, static_cast<float>(unit.attackRange),
                                 Fade(RED, 0.35f));
            }
        }
        if (unit.controlGroups != 0)
        {
            // QoL control-group badge: lowest group number, above the bar
            // (same sprite-following box, so it never lands on a soldier).
            int lowestBit = 0;
            while (lowestBit < 9 && (unit.controlGroups & (1u << lowestBit)) == 0)
            {
                ++lowestBit;
            }
            const Rectangle badgeBox = SquadSelectionBox(
                art, unit, id, !(art.UseRectangles() && !art.UseAtlas()), body);
            Art::DrawUiText(&art, TextFormat("%d", (lowestBit + 1) % 10), static_cast<int>(badgeBox.x),
                     static_cast<int>(badgeBox.y) - 14, 12, DARKBLUE);
        }
        if (unit.hitFlashTime > 0.0f)
        {
            DrawRectangleRec(body, Fade(WHITE, 0.7f));
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
    // Particles in world space, under the shroud so hidden battles
    // stay hidden.
    art.ParticlesPool().Draw();
    damageNumbers.Draw(); // same world-space block, same shroud rule
    // Shroud, drawn over the world: unexplored tiles go opaque black,
    // explored-but-unseen tiles get a dim veil (frozen snapshot).
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
}

// HUD + overlays (H6 slice 3b of Update): drag previews, minimap blit,
// panels, tooltips, menu overlays, transition fade. Screen space, after
// DrawWorld, inside the caller's Begin/EndDrawing pair.
void Game::DrawHudAndOverlays(int screenWidth, int screenHeight)
{
    // Drag-box visual (screen space, under the HUD panels).
    if (playingInput.IsDragging() && input.LeftDown())
    {
        DrawRectangleLinesEx(
            NormalizeRect(playingInput.DragStart(), input.MouseScreen()), 1.0f, GREEN);
    }
    // QoL area-repair preview (screen space, same layer as drag-box):
    // live zone rect plus a marker on every damaged candidate that would
    // be assigned on release.
    if (playingInput.RepairDragActive() && input.LeftDown() && playingInput.AreaRepairMode())
    {
        DrawRectangleLinesEx(
            NormalizeRect(playingInput.RepairDragStart(), input.MouseScreen()), 1.0f, GREEN);
        std::vector<Entity> preview;
        CollectAreaRepairCandidates(
            registry,
            DraggedWorldBox(camera, playingInput.RepairDragStart(), input.MouseScreen()), 0,
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
    if (playingInput.IsRightDragging() && input.RightDown())
    {
        DrawLineEx(playingInput.RightDragStart(), input.MouseScreen(), 2.0f, SKYBLUE);
        DrawCircleV(playingInput.RightDragStart(), 3.0f, SKYBLUE);
        DrawCircleV(input.MouseScreen(), 3.0f, SKYBLUE);
    }

    // Minimap blit (texture is Y-flipped) + viewport box.
    // Hidden from the settings panel.
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

    // Raygui HUD (proper panels replace the text counters).
    DrawResourcePanel(resources, &art);
    DrawSelectionPanel(registry, &art);
    DrawIdleButtons(registry, 0); // QoL: team 0 is the player
    DrawRepairPanel(&playerAutoRepair, &autoRepairCap);
    DrawControlGroupStrip(registry, 0, playingInput.AutoAddGroupBit(),
                            &art); // QoL: team 0 is the player
    DrawSaveSlots();
    // Factory panel (build buttons, queue, cancel); rally hint
    // while placing the rally point. Recomputed here (not just the sim
    // gate above) so the panel stays correct while paused.
    sim.RefreshFactory();
    DrawProductionPanel(resources, queue, sim.HasFactory());
    if (playingInput.IsSettingRally())
    {
        Art::DrawUiText(&art, "Rally: left-click to place (R cancels)", 250, 364, 16, DARKGREEN);
    }
    if (playingInput.AttackGroundMode())
    {
        Art::DrawUiText(&art, "Shelling: right-click to fire (X/Esc cancels)", 250, 364, 16, RED);
    }
    if (!queue.Empty())
    {
        const float ppw = 174.0f;
        const float ppx = static_cast<float>(screenWidth) - ppw - 10.0f;
        Art::DrawUiText(&art, "Producing...", static_cast<int>(ppx), screenHeight - 222, 16, GRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, 150, 12, LIGHTGRAY);
        DrawRectangle(static_cast<int>(ppx), screenHeight - 202, static_cast<int>(150.0f * queue.HeadProgress()), 12, DARKGREEN);
    }

    // Live frame-rate readout (60 FPS target validation).
    DrawFPS(screenWidth - 170, 135);
    // Enemy commander status (: difficulty comes from the setup).
    Art::DrawUiText(&art, TextFormat("Enemy: %s  Waves: %d", DifficultyName(worldDifficulty),
                        ai.WavesLaunched()),
             screenWidth - 170, 155, 16, GRAY);

    // Shortcut overlay, bottom-left, toggled with F1.
    if (showHints)
    {
        const std::vector<std::string> hints = ShortcutHintLines(hotkeys);
        for (std::size_t i = 0; i < hints.size(); ++i)
        {
            Art::DrawUiText(&art, hints[i].c_str(), 8, 250 + static_cast<int>(i) * 18, 14, Fade(DARKGRAY, 0.8f));
        }
    }

    // Hover tooltip: stat block for the unit under the cursor after a short
    // hold. Playing only; suppressed while an order/drag/placement gesture
    // is in flight so it never collides with the selection box or ghost.
    // Enemies show only under live fog (no scouting through the shroud).
    if (menu.state == MenuState::Playing && !playingInput.IsDragging() &&
        !playingInput.PlaceDragActive() && !playingInput.RepairDragActive() &&
        !playingInput.IsRightDragging() && !playingInput.PlacingType().has_value() &&
        !playingInput.AttackGroundMode() && !playingInput.AreaRepairMode())
    {
        const Entity hovered = PickUnitAt(registry, input.MouseWorld(camera));
        if (UpdateHoverTooltip(hoverTip, hovered, GetFrameTime(), kHoverTooltipDelay))
        {
            if (const Unit *unit = registry.Get<Unit>(hoverTip.hovered))
            {
                const bool visible =
                    unit->teamID == 0 ||
                    fog.IsVisible(0, cc::WorldToTile(cc::ToGlm(unit->position)));
                if (visible)
                {
                    const std::vector<std::string> lines = UnitTooltipLines(*unit);
                    int width = 0;
                    for (const std::string &line : lines)
                    {
                        width = std::max(width, MeasureText(line.c_str(), 14));
                    }
                    const Vector2 mouse = input.MouseScreen();
                    const int tx = static_cast<int>(mouse.x) + 16;
                    const int ty = static_cast<int>(mouse.y) + 20;
                    DrawRectangle(tx - 4, ty - 4, width + 8,
                                  static_cast<int>(lines.size()) * 18 + 4, Fade(BLACK, 0.75f));
                    for (std::size_t i = 0; i < lines.size(); ++i)
                    {
                        Art::DrawUiText(&art, lines[i].c_str(), tx, ty + static_cast<int>(i) * 18, 14,
                                 RAYWHITE);
                    }
                }
            }
        }
    }
    else
    {
        hoverTip.hovered = kInvalidEntity;
        hoverTip.time = 0.0f;
    }

    // Menu overlays sit on top of the frame.
    if (menu.state == MenuState::HotkeyRemap)
    {
        // Entered from pause (world stays frozen: the sim only advances in
        // Playing). Same screen as the Settings-chain branch above.
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        menuScreens.DrawRemap(screenWidth / 2.0f);
    }
    if (menu.state == MenuState::ReplayViewer)
    {
        // QoL replay banner (no window: the world render stays visible).
        // Snapshot slideshow, not a re-simulation — production queues were
        // never saved, so units appear at snapshot boundaries.
        Art::DrawUiText(&art, TextFormat("Replay %d/%d", replayCursor + 1, sim.ReplayCount()), 8, 96, 16,
                 DARKGRAY);
        Art::DrawUiText(&art, "Left/Right step - Esc exit", 8, 116, 14, Fade(DARKGRAY, 0.8f));
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
        // Volumes (0..1) + mute, persisted by the settings file.
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
            menuScreens.BeginRemap(MenuState::Paused);
        }
        // Pause shares MenuSettings with the settings screen; leaving
        // via Back-equivalent persists, pause buttons apply live.
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
    // Same transition fade as the menu branch: covers Playing entry and the
    // Paused/Victory/GameOver overlays (all flow through menu.state).
    if (const float fade = MenuFadeAlpha(menuStateTime); fade < 1.0f)
    {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 1.0f - fade));
    }
}

void Game::Update()
{
    // Single input pump (always runs: P must unpause too).
    // Camera speed is a live menu setting.
    input.Update(camera, menu.settings.cameraSpeed, GetFrameTime());
    // Scroll-wheel zoom (clamped in GameCamera) runs even paused.
    // Zoom-out floor follows the world: at min zoom the map exactly fills
    // the screen, so over-zooming can never show void past the edge.
    camera.AdjustZoom(input.WheelDelta());
    // Resizable window — refresh live dims, keep the camera centered
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
    // Menu transition clock: edge-detects state changes for the fade-in
    // overlay (both menu screens and in-world overlays share menu.state).
    TrackMenuTransition(previousMenuState, menuStateTime, menu.state, GetFrameTime());

    // Menu branch — no world simulates or renders until Start (or a
    // slot load). The match code below runs untouched once worldActive.
    if (!worldActive)
    {
        menuScreens.Draw(screenWidth, screenHeight, menuStateTime);
        return;
    }

    // Orders, AI, economy, and minimap only advance while
    // Playing; rendering below always runs so menus overlay a live frame.
    if (menu.state == MenuState::Playing)
    {
        playingInput.Dispatch();
        // Sim tick: visibility, movement, deaths, economy, production,
        // AI, minimap refresh, outcome (see Simulation::Step).
        sim.Step(GetFrameTime());
    }

    // Volumes follow the pause-menu sliders live; the loop streams on.
    audio.ApplySettings(menu.settings.masterVolume, menu.settings.musicVolume,
                        menu.settings.sfxVolume, menu.settings.mute);
    audio.UpdateMusic();

    // QoL replay viewer auto-advance (outside the Playing sim gate: the
    // viewer never simulates). Steps at the recording cadence; holds on
    // the last frame instead of wrapping.
    if (menu.state == MenuState::ReplayViewer && sim.ReplayCount() > 0)
    {
        replayPlayTimer += GetFrameTime();
        if (replayPlayTimer >= 2.0f)
        {
            replayPlayTimer = 0.0f;
            if (replayCursor + 1 < sim.ReplayCount())
            {
                StepReplay(1);
            }
        }
    }
    pings.Update(GetFrameTime()); // QoL: UI clock — ages in menus/viewer too
    shakeTrauma = DecayShakeTrauma(shakeTrauma, GetFrameTime());

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawWorld();

    DrawHudAndOverlays(screenWidth, screenHeight);
    EndDrawing();
}

void Game::Shutdown()
{
    minimap.Unload();
    art.Shutdown(); // Unload sprite textures
    audio.Shutdown(); // Unload sounds + music stream
    CloseAudioDevice();
    CloseWindow();
    Log::Shutdown(); // last: keep the log sink through CloseWindow chatter
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
