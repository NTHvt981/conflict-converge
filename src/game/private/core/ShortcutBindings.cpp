#include "core/ShortcutBindings.h"

#include "economy/Building.h"
#include "units/Extensions.h"
#include "app/input/Selection.h"
#include "app/input/Shortcuts.h"
#include "units/Unit.h"
#include "units/UnitCommands.h"

ShortcutBindings::ShortcutBindings(InputManager &input, HotkeyMap &hotkeys, MenuFlow &menu,
                                   RmlUiMenus &rmlUiMenus,
                                   PlayingInput &playingInput, Audio &audio, GameCamera &camera,
                                   TileMap &map, OccupancyGrid &occ, ResourceNodes &nodes,
                                   FogOfWar &fog, Registry &registry, ResourceSystem &resources,
                                   EventDispatcher &events, const WorldState &worldState,
                                   Pings &pings, const bool &worldActive, bool &showHints,
                                   std::function<void()> quitToMenu,
                                   std::function<void(int)> stepReplay)
    : input_(input)
    , hotkeys_(hotkeys)
    , menu_(menu)
    , rmlUiMenus_(rmlUiMenus)
    , playingInput_(playingInput)
    , audio_(audio)
    , camera_(camera)
    , map_(map)
    , occ_(occ)
    , nodes_(nodes)
    , fog_(fog)
    , registry_(registry)
    , resources_(resources)
    , events_(events)
    , worldState_(worldState)
    , pings_(pings)
    , worldActive_(worldActive)
    , showHints_(showHints)
    , quitToMenu_(std::move(quitToMenu))
    , stepReplay_(std::move(stepReplay))
{
}

void ShortcutBindings::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
}

void ShortcutBindings::Bind()
{
    input_.shortcuts.Clear();
    input_.shortcuts.Bind(hotkeys_.KeyFor("ToggleHints"), [&] { showHints_ = !showHints_; });
    input_.shortcuts.Bind(hotkeys_.KeyFor("TogglePause"), [&] {
        if (menu_.state == MenuState::Playing)
        {
            menu_.TogglePause();
            Announce(EventType::MenuAction);
            Announce(EventType::MatchPaused);
        }
        else if (menu_.state == MenuState::Paused)
        {
            menu_.TogglePause();
            Announce(EventType::MenuAction);
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("Quicksave"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        SaveWorld({ &registry_, &resources_, &map_, &camera_, &nodes_, &fog_, &occ_ }, "data/quicksave.ccpb");
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("Quickload"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        LoadWorld({ &registry_, &resources_, &map_, &camera_, &nodes_, &fog_, &occ_ }, "data/quicksave.ccpb");
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("SaveSlot1"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            SaveWorld(worldState_, SaveSlotPath(1));
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("SaveSlot2"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            SaveWorld(worldState_, SaveSlotPath(2));
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("SaveSlot3"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            SaveWorld(worldState_, SaveSlotPath(3));
        }
    });
    input_.shortcuts.BindChord(hotkeys_.KeyFor("LoadSlot1"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            LoadWorld(worldState_, SaveSlotPath(1));
        }
    });
    input_.shortcuts.BindChord(hotkeys_.KeyFor("LoadSlot2"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            LoadWorld(worldState_, SaveSlotPath(2));
        }
    });
    input_.shortcuts.BindChord(hotkeys_.KeyFor("LoadSlot3"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            LoadWorld(worldState_, SaveSlotPath(3));
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("AttackMove"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        const Vector2 dest = input_.MouseWorld(camera_);
        const bool queued = input_.ShiftDown();
        const int acted = IssueSelectionAttackMove(registry_, map_, &occ_, dest, queued);
        if (acted > 0)
        {
            audio_.Play(SfxId::Confirm);
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("StanceHold"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        SetSelectionStance(registry_, Stance::Hold);
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("StanceGuard"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        SetSelectionStance(registry_, Stance::Guard);
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("Patrol"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        const Vector2 dest = input_.MouseWorld(camera_);
        const bool queued = input_.ShiftDown();
        const int acted = IssueSelectionPatrol(registry_, map_, &occ_, dest, queued);
        if (acted > 0)
        {
            audio_.Play(SfxId::Confirm);
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("Rally"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            playingInput_.ToggleSettingRally();
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("SelectType"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        const Entity selected = SelectedUnit(registry_);
        const Unit *unit = registry_.Get<Unit>(selected);
        if (unit != nullptr)
        {
            SelectAllOfType(registry_, unit->type, 0, false);
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("SelectFactories"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        SelectAllBuildings(registry_, BuildingType::Factory, 0);
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("SlowestSpeed"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            playingInput_.ToggleSlowestSpeed();
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("AreaBuild"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            playingInput_.ToggleAreaBuild();
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("AreaRepair"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            playingInput_.ToggleAreaRepair();
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("AttackGround"), [&] {
        if (worldActive_ && menu_.state == MenuState::Playing)
        {
            playingInput_.ToggleAttackGround();
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("AutoRetreat"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        ToggleSelectionAutoRetreat(registry_);
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("JumpPing"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        Vector2 pingPos = {};
        if (pings_.Latest(pingPos))
        {
            camera_.view.target = pingPos;
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("ReplayBack"), [&] {
        if (menu_.state == MenuState::ReplayViewer)
        {
            stepReplay_(-1);
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("ReplayFwd"), [&] {
        if (menu_.state == MenuState::ReplayViewer)
        {
            stepReplay_(1);
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("Back"), [&] {
        // The RML remap screen owns Esc while it owns the HotkeyRemap state.
        if (rmlUiMenus_.CancelRemapCapture())
        {
            return;
        }
        switch (menu_.OnBackPressed())
        {
        case BackAction::Navigate:
        case BackAction::OpenQuitConfirm:
        case BackAction::OpenBackToMenuConfirm:
            Announce(EventType::MenuAction);
            break;
        case BackAction::QuitToMenu:
            quitToMenu_();
            break;
        case BackAction::None:
        default:
            break;
        }
    });
    input_.shortcuts.Bind(hotkeys_.KeyFor("Halt"), [&] {
        if (!worldActive_ || menu_.state != MenuState::Playing)
        {
            return;
        }
        HaltSelection(registry_);
    });
}
