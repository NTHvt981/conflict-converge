#pragma once

#include <string>

#include "AICommander.h"
#include "Event.h"
#include "GameCamera.h"
#include "Hotkeys.h"
#include "Menu.h"
#include "MenuScreens.h"
#include "Minimap.h"
#include "PlayingInput.h"
#include "SaveGame.h"
#include "Simulation.h"
#include "Skirmish.h"
#include "Subsystem.h"
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

    Subsystems &world_; // match-boundary ResetForMatch fan-out (world scope)
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
