#pragma once

#include <string>

#include "raylib.h"
#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "Hotkeys.h"
#include "Hud.h" // hover-tooltip debounce state (pure, headless-safe)
#include "InputManager.h"
#include "MapFile.h"
#include "Menu.h"
#include "MenuScreens.h" // out-of-world menu branch (H6 follow-up extraction)
#include "Minimap.h"
#include "Nodes.h"
#include "Pings.h"
#include "PlayingInput.h" // in-match gesture dispatch (H6 follow-up extraction)
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "Shake.h" // trauma-pattern screen shake (pure helpers, header-only)
#include "Simulation.h" // per-frame match tick (H6 follow-up extraction)
#include "Skirmish.h"
#include "TileMap.h"
#include "UnitFactory.h"

// Game owns the former main.cpp boot state + frame loop. main.cpp is only
// the entry point (construct, Run, return). Members are declared in
// construction-dependency order: factory/ai/skirmish/worldState hold
// references/pointers into the registries above them, so reordering breaks
// them. Non-copyable/non-movable for the same reason (shortcut lambdas
// capture this).
class Game
{
public:
    Game();
    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    // Lifecycle (previously the sections of main).
    void Init();     // window, audio/art, state, shortcut bindings
    void Update();   // exactly one frame (input pump + sim + render)
    void Shutdown(); // unload + CloseWindow
    bool IsRunning() const;
    // Convenience for main: Init + loop + Shutdown.
    int Run();

    // E2E driver seam (tests/e2e only): the hidden-window harness drives
    // the real loop without a human. The game and unit tests never touch
    // these; they forward to the same private paths the GUI buttons use.
    MenuFlow &E2EMenu();
    Registry &E2ERegistry();
    TileMap &E2EMap();
    bool IsWorldActive() const;
    // SelectedMap + difficulty -> StartMatch; false when nothing selected.
    bool E2EStartSelectedMatch();
    void E2EQuitToMenu();

private:
    void Announce(EventType type);
    void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
    void QuitToMenu();
    // Slot-load apply for the menu load screen (world rebuild over a
    // fresh shell + commander re-arm; the slot UI lives in MenuScreens).
    void LoadGameFromSlot(const std::string &slotPath);
    void BindShortcuts();
    // QoL snapshot replay viewer: step the loaded snapshot cursor
    // (clamped, minimap poked). Enter via WatchLastReplay.
    void StepReplay(int dir);
    bool WatchLastReplay();
    // H6 decomposition of Update (each verbatim-extracted, no behavior
    // change): world draw, HUD/overlay draw. The sim tick graduated to
    // Simulation, the menu branch to MenuScreens, the input dispatch to
    // PlayingInput (own files).
    void DrawWorld();
    void DrawHudAndOverlays(int screenWidth, int screenHeight);

    // Former boot-level locals in main, same order.
    Audio audio;
    Art art;
    GameCamera camera;
    MenuFlow menu;
    Minimap minimap;
    Pings pings; // QoL attack/event pings (minimap blips + camera jump)
    Registry registry;
    ResourceSystem resources;
    EventDispatcher events;
    TileMap map;
    OccupancyGrid occ; // Unit/building tile occupancy
    FogOfWar fog;
    ResourceNodes nodes;
    ProductionQueue queue;
    UnitFactory factory;
    AICommander ai;
    // 2v2 overflow commanders: allied (team 0) + second enemy (team 1).
    // Declared after ai (same reference dependencies); parked by
    // ResetSkirmish and armed by BuildSkirmish only on 2v2 maps.
    AICommander allyAI;
    AICommander enemyAI2;
    Vector2 rallyPos = {};
    SkirmishWorld skirmish;
    WorldState worldState;
    InputManager input;
    // QoL remappable hotkeys (Tier 1): effective keys for BindShortcuts.
    // Loaded from settings at startup, rebound live from the remap screen
    // (capture state lives in MenuScreens).
    HotkeyMap hotkeys;

    bool worldActive = false;
    // 2v2 overflow commanders tick only in 2v2 matches. NEVER gate them on
    // unit counts: parked commanders share teams with live units (allyAI is
    // team 0 like the player), so a count guard wakes them in 1v1 — where
    // the ally would then order the player's army and spawn free units at
    // the origin (found by e2e: phantom scouts at (0,3), hijacked mop-up).
    bool worldIs2v2 = false;
    // Prototype sandbox: no AI, no bases, no win/lose — one player squad
    // on terrain. Set when the started map has a '1' but no '2' marker.
    bool sandboxMode = false;
    AIDifficulty worldDifficulty = AIDifficulty::Medium;
    std::string worldMapPath;

    // QoL snapshot replay viewer cursor (recording state lives in
    // Simulation: only the tick records, the viewer only reads).
    int replayCursor = 0; // currently shown frame
    float replayPlayTimer = 0.0f; // auto-advance clock in the viewer
    bool showHints = true;
    // QoL building auto-repair (player-global v1: no building selection
    // exists yet for per-building toggles). Cap scales the repair rate.
    bool playerAutoRepair = false;
    float autoRepairCap = 1.0f;
    // Per-frame render state (sim edge-trigger polls moved to Simulation).
    HoverTooltipState hoverTip; // unit hover-tooltip debounce (Playing only)
    float shakeTrauma = 0.0f; // screen-shake trauma 0..1 (render copy only)
    DamageNumbers damageNumbers; // floating hit numbers (presentation-only, unsaved)
    MenuState previousMenuState = MenuState::MainMenu; // transition-fade clock
    float menuStateTime = 0.0f;
    MenuState lastOutcomeState = MenuState::MainMenu;
    // Input dispatch + match tick (declared after the members they bind;
    // menu screens last: same ordering rule).
    PlayingInput playingInput;
    Simulation sim;
    MenuScreens menuScreens;
};
