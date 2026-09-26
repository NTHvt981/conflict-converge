#include "core/MatchController.h"

#include "world/MapFile.h"
#include "app/Skirmish.h"
#include "app/SaveGame.h"
#include "app/UnitConfig.h"
#include <filesystem>

MatchController::MatchController(Subsystems &world, GameCamera &camera, AICommander &ai, AICommander &allyAI,
                                 AICommander &enemyAI2, MenuFlow &menu, Minimap &minimap,
                                 PlayingInput &playingInput, Simulation &sim,
                                 EventDispatcher &events, SkirmishWorld &skirmish,
                                 WorldState &worldState, HotkeyMap &hotkeys, Vector2 &rallyPos,
                                 bool &worldActive, bool &worldIs2v2, bool &sandboxMode,
                                 AIDifficulty &worldDifficulty, std::string &worldMapPath,
                                 MenuState &lastOutcomeState, int &replayCursor,
                                 float &replayPlayTimer, ConfirmChoice &pendingConfirm)
    : world_(world)
    , camera_(camera)
    , ai_(ai)
    , allyAI_(allyAI)
    , enemyAI2_(enemyAI2)
    , menu_(menu)
    , minimap_(minimap)
    , playingInput_(playingInput)
    , sim_(sim)
    , events_(events)
    , skirmish_(skirmish)
    , worldState_(worldState)
    , hotkeys_(hotkeys)
    , rallyPos_(rallyPos)
    , worldActive_(worldActive)
    , worldIs2v2_(worldIs2v2)
    , sandboxMode_(sandboxMode)
    , worldDifficulty_(worldDifficulty)
    , worldMapPath_(worldMapPath)
    , lastOutcomeState_(lastOutcomeState)
    , replayCursor_(replayCursor)
    , replayPlayTimer_(replayPlayTimer)
    , pendingConfirm_(pendingConfirm)
{
}

void MatchController::Announce(EventType type)
{
    Event bare;
    bare.type = type;
    events_.Dispatch(bare);
}

void MatchController::StartMatch(const std::string &mapPath, AIDifficulty difficulty)
{
    RefreshActiveUnitConfigsFromSearch();
    MapData startData;
    sandboxMode_ = ParseMapFile(mapPath, startData) && !startData.playerSpawns.empty() &&
                   startData.aiSpawns.empty();
    if (sandboxMode_)
    {
        BuildSandbox(skirmish_, mapPath);
    }
    else
    {
        BuildSkirmish(skirmish_, mapPath, difficulty);
    }
    worldMapPath_ = mapPath;
    worldDifficulty_ = difficulty;
    worldActive_ = true;
    worldIs2v2_ = !sandboxMode_ && SpotsForMap(mapPath).is2v2;
    playingInput_.ResetForMatch();
    lastOutcomeState_ = MenuState::Playing;
    camera_.view.zoom = 1.0f;
    minimap_.elapsed = minimap_.refreshInterval;
    sim_.ResetForMatch();
    world_.ResetForMatch();
    replayCursor_ = 0;
    replayPlayTimer_ = 0.0f;
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

void MatchController::QuitToMenu()
{
    ResetSkirmish(skirmish_);
    world_.ResetForMatch();
    worldActive_ = false;
    worldIs2v2_ = false;
    sandboxMode_ = false;
    sim_.StopRecording();
    Announce(EventType::MenuAction);
    menu_.OpenMainMenu();
}

void MatchController::PollConfirmKeys()
{
    if (!menu_.ConfirmOpen())
    {
        return;
    }
    if (IsKeyPressed(KEY_Y))
    {
        pendingConfirm_ = ConfirmChoice::Yes;
        return;
    }
    const int backKey = hotkeys_.KeyFor("Back");
    if (IsKeyPressed(KEY_N) || (backKey != 0 && IsKeyPressed(backKey)))
    {
        pendingConfirm_ = ConfirmChoice::No;
    }
}

void MatchController::ApplyConfirmChoice(ConfirmChoice choice)
{
    if (choice == ConfirmChoice::None)
    {
        return;
    }
    const ConfirmKind kind = menu_.confirm;
    menu_.CloseConfirm();
    if (choice == ConfirmChoice::Yes)
    {
        if (kind == ConfirmKind::QuitApp)
        {
            menu_.quitRequested = true;
        }
        else if (kind == ConfirmKind::BackToMenu)
        {
            QuitToMenu();
        }
    }
}

void MatchController::LoadGameFromSlot(const std::string &slotPath)
{
    RefreshActiveUnitConfigsFromSearch();
    ResetSkirmish(skirmish_);
    if (!LoadWorld(worldState_, slotPath))
    {
        return;
    }
    const SkirmishSpots spots =
        SpotsForMap(worldMapPath_.empty() ? "data/maps/crossroads.map" : worldMapPath_);
    ai_.Reset(menu_.setup.difficulty, spots.aiHome, spots.playerHome, 1);
    if (spots.is2v2)
    {
        allyAI_.Reset(menu_.setup.difficulty, spots.allyHome, spots.aiHome, 0);
        enemyAI2_.Reset(menu_.setup.difficulty, spots.enemyHome2, spots.playerHome, 1);
    }
    rallyPos_ = camera_.view.target;
    worldDifficulty_ = menu_.setup.difficulty;
    worldActive_ = true;
    worldIs2v2_ = spots.is2v2;
    playingInput_.ResetForMatch();
    sim_.ResetEdgePolls();
    world_.ResetForMatch();
    lastOutcomeState_ = MenuState::Playing;
    camera_.view.zoom = 1.0f;
    minimap_.elapsed = minimap_.refreshInterval;
    menu_.state = MenuState::Playing;
    Announce(EventType::MenuAction);
    Announce(EventType::MatchStarted);
}

void MatchController::StepReplay(int dir)
{
    if (sim_.ReplayCount() <= 0)
    {
        return;
    }
    replayCursor_ += dir;
    if (replayCursor_ < 0)
    {
        replayCursor_ = 0;
    }
    if (replayCursor_ >= sim_.ReplayCount())
    {
        replayCursor_ = sim_.ReplayCount() - 1;
    }
    if (LoadWorld(worldState_, ReplayFramePath(kReplayDir, replayCursor_)))
    {
        replayPlayTimer_ = 0.0f;
        minimap_.elapsed = minimap_.refreshInterval;
    }
}

bool MatchController::WatchLastReplay()
{
    const int count = ReplayFrameCount(kReplayDir);
    if (count <= 0)
    {
        return false;
    }
    ResetSkirmish(skirmish_);
    if (!LoadWorld(worldState_, ReplayFramePath(kReplayDir, 0)))
    {
        return false;
    }
    sim_.StopRecording();
    sim_.SetReplayCount(count);
    replayCursor_ = 0;
    replayPlayTimer_ = 0.0f;
    worldActive_ = true;
    worldIs2v2_ = false;
    playingInput_.ResetForMatch();
    world_.ResetForMatch();
    camera_.view.zoom = 1.0f;
    minimap_.elapsed = minimap_.refreshInterval;
    menu_.state = MenuState::ReplayViewer;
    Announce(EventType::MenuAction);
    return true;
}
