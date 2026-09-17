#pragma once

#include <optional> // QoL placement flow (empty = not placing)
#include <string>

#include "raylib.h"
#include "AICommander.h"
#include "Art.h"
#include "Audio.h"
#include "Building.h" // BuildingType for the placement flow
#include "Event.h"
#include "FogOfWar.h"
#include "GameCamera.h"
#include "Hotkeys.h"
#include "Hud.h" // hover-tooltip debounce state (pure, headless-safe)
#include "InputManager.h"
#include "MapFile.h"
#include "Menu.h"
#include "Minimap.h"
#include "Nodes.h"
#include "Pings.h"
#include "Production.h"
#include "Registry.h"
#include "ResourceSystem.h"
#include "SaveGame.h"
#include "Shake.h" // trauma-pattern screen shake (pure helpers, header-only)
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

    // Lifecycle (previously the sections of main()).
    void Init();     // window, audio/art, state, shortcut bindings
    void Update();   // exactly one frame (input pump + sim + render)
    void Shutdown(); // unload + CloseWindow
    bool IsRunning() const;
    // Convenience for main(): Init + loop + Shutdown.
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
    void BindShortcuts();
    // QoL hotkey remap: push settings overrides into `hotkeys` (then
    // re-run BindShortcuts), and pull live overrides back into settings
    // before every SaveSettings so the file stays in sync.
    void ApplyHotkeyOverrides();
    void SyncHotkeySettings();
    // QoL remap screen body (shared by the Settings-chain branch and the
    // paused overlay: same screen, whichever state entered it).
    void DrawHotkeyRemap(float cx);
    // QoL snapshot replay viewer: step the loaded snapshot cursor
    // (clamped, minimap poked). Enter via WatchLastReplay.
    void StepReplay(int dir);
    bool WatchLastReplay();

    // Former boot-level locals in main(), same order.
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
    OccupancyGrid occ; // Phase 4: unit/building tile occupancy
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
    // Loaded from settings at startup, rebound live from the remap screen.
    HotkeyMap hotkeys;
    // QoL remap-screen capture state: action index being rebound (-1 =
    // none), pending conflict (action + key awaiting second-click
    // confirm), and where Esc returns to (Settings or Paused).
    int remapArming = -1;
    std::string remapConflictAction;
    int remapConflictKey = 0;
    MenuState remapReturn = MenuState::Settings;

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

    bool settingRally = false;
    bool dragging = false;
    Vector2 dragStart = {};
    // QoL line formation: Alt+right-drag draws a placement line instead of
    // issuing a point order (mirrors dragging/dragStart for the left button).
    bool rightDragging = false;
    Vector2 rightDragStart = {};
    // QoL right-drag pan (opt-in): press defers the click order until
    // release decides click (order) vs. drag (pan), past a pixel threshold.
    bool pendingRightClick = false;
    float rightDragDist = 0.0f;
    // QoL snapshot replay recording (this match) + viewer cursor.
    bool replayRecording = false;
    float replayTimer = 0.0f;
    int replayIndex = 0;
    int replayCount = 0; // frames available for the viewer
    int replayCursor = 0; // currently shown frame
    float replayPlayTimer = 0.0f; // auto-advance clock in the viewer
    // QoL map editor scratch state (24x18 canvas, never the live match).
    MapData editorMap;
    char editorBrush = '.';
    std::string editorStatus;
    bool showHints = true;
    int setupScroll = 0;
    // QoL control groups: production auto-joins this group bit when >= 0
    // (set via Ctrl+Shift+number, cleared by... nothing — persists until
    // rebound; -1 = off).
    int autoAddGroupBit = -1;
    // QoL attack-ground mode (toggled with X): the next right-click shells
    // the point instead of moving. One-shot, clears after a single use.
    bool attackGroundMode = false;
    // QoL building auto-repair (player-global v1: no building selection
    // exists yet for per-building toggles). Cap scales the repair rate.
    bool playerAutoRepair = false;
    float autoRepairCap = 1.0f;
    // QoL move-at-slowest-speed: formation orders march at the squad
    // minimum while true (toggled with B).
    bool moveAtSlowestSpeed = false;
    // QoL area-build placement flow: some building while engaged (Z
    // toggles, 1/2/3 picks the type, left-click/drag places, right-click
    // or Esc cancels). Empty = not placing. Stays engaged across
    // placements so rows of structures go down fast.
    std::optional<BuildingType> placingType;
    bool placeDragActive = false;
    Vector2 placeDragStart = {};
    // QoL area repair (toggled with E): drag a zone, every selected
    // Engineer repairs its nearest damaged target inside it. Left-drag
    // sibling of area-build (own mode + drag state, never shared).
    bool areaRepairMode = false;
    bool repairDragActive = false;
    Vector2 repairDragStart = {};

    // Per-frame poll state (M11 edge-triggered sounds, M14 transitions).
    int lastBuildingCount = 0;
    int lastDepletedCount = 0;
    int lastQueueSize = 0;
    float attackSfxTimer = 0.0f;
    HoverTooltipState hoverTip; // unit hover-tooltip debounce (Playing only)
    float shakeTrauma = 0.0f; // screen-shake trauma 0..1 (render copy only)
    DamageNumbers damageNumbers; // floating hit numbers (presentation-only, unsaved)
    MenuState lastOutcomeState = MenuState::MainMenu;
    bool hasFactory = false;
};
