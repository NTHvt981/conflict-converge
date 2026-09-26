#pragma once

#include <string>

#include "units/AICommander.h"
#include "core/Event.h"
#include "app/ui/GameCamera.h"
#include "app/input/Hotkeys.h"
#include "app/match/Menu.h"
#include "core/MenuScreens.h"
#include "app/ui/Minimap.h"
#include "core/PlayingInput.h"
#include "app/save/SaveGame.h"
#include "core/Simulation.h"
#include "app/match/Skirmish.h"
#include "core/Subsystem.h"
#include "raylib.h"

class MatchController
{
public:
    MatchController(Subsystems &world, GameCamera &camera, AICommander &ai, AICommander &allyAI,
                    AICommander &enemyAI2, MenuFlow &menu, Minimap &minimap,
                    PlayingInput &playingInput, Simulation &sim, EventDispatcher &events,
                    SkirmishWorld &skirmish, WorldState &worldState, HotkeyMap &hotkeys,
                    Vector2 &rallyPos, bool &worldActive, bool &worldIs2v2,
                    bool &sandboxMode, AIDifficulty &worldDifficulty,
                    std::string &worldMapPath, MenuState &lastOutcomeState,
                    int &replayCursor, float &replayPlayTimer, ConfirmChoice &pendingConfirm);
    MatchController(const MatchController &) = delete;
    MatchController &operator=(const MatchController &) = delete;

    void StartMatch(const std::string &mapPath, AIDifficulty difficulty);
    void QuitToMenu();
    void LoadGameFromSlot(const std::string &slotPath);
    void StepReplay(int dir);
    bool WatchLastReplay();
    void PollConfirmKeys();
    void ApplyConfirmChoice(ConfirmChoice choice);

private:
    void Announce(EventType type);

    Subsystems &world_;
    GameCamera &camera_;
    AICommander &ai_;
    AICommander &allyAI_;
    AICommander &enemyAI2_;
    MenuFlow &menu_;
    Minimap &minimap_;
    PlayingInput &playingInput_;
    Simulation &sim_;
    EventDispatcher &events_;
    SkirmishWorld &skirmish_;
    WorldState &worldState_;
    HotkeyMap &hotkeys_;
    Vector2 &rallyPos_;
    bool &worldActive_;
    bool &worldIs2v2_;
    bool &sandboxMode_;
    AIDifficulty &worldDifficulty_;
    std::string &worldMapPath_;
    MenuState &lastOutcomeState_;
    int &replayCursor_;
    float &replayPlayTimer_;
    ConfirmChoice &pendingConfirm_;
};
