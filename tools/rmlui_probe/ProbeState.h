#pragma once

// Mock menu state for the RmlUi probe (Step 1). Mirrors the shapes in
// src/game/public/app/Menu.h (MenuFlow/MenuSettings/SkirmishSetup) and the
// label set of kHotkeyDefs in src/game/private/app/Hotkeys.cpp, without
// linking any src/game TU. Pure logic, no RmlUi.

#include <string>
#include <vector>

enum class ProbePage
{
    MainMenu,
    SkirmishSetup,
    Settings,
    HotkeyRemap,
    LoadGame,
    Playing,
    Paused,
    GameOver,
    Victory
};

enum class ProbeConfirm
{
    None,
    QuitApp,   // "Quit Conflict Converge?" modal
    BackToMenu // "Return to main menu?" modal
};

// Mirrors BackAction in Menu.h.
enum class ProbeBackAction
{
    None,                 // consumed (e.g. the modal was cancelled)
    Navigate,             // state already changed
    QuitToMenu,           // host tears down the world, then main menu
    OpenQuitConfirm,      // modal opened: quit-to-desktop
    OpenBackToMenuConfirm // modal opened: back-to-menu
};

enum class ProbeDifficulty
{
    Easy = 0,
    Medium = 1,
    Hard = 2
};

struct ProbeMapEntry
{
    std::string name;
    std::string author;
    std::string path;
    int width = 0;
    int height = 0;
};

// Display strings follow HotkeyDisplayName() in Hotkeys.cpp
// ("F1", "Left", "Esc", "Space", "Shift+" prefix for chords).
struct ProbeHotkeyDef
{
    const char* action;
    const char* label;
    const char* key;
    bool chord;
};

inline const ProbeHotkeyDef kProbeHotkeyDefs[] = {
    { "ToggleHints", "Shortcut hints", "F1", false },
    { "TogglePause", "Pause / resume", "P", false },
    { "Quicksave", "Quicksave", "F5", false },
    { "Quickload", "Quickload", "F9", false },
    { "SaveSlot1", "Save slot 1", "F6", false },
    { "SaveSlot2", "Save slot 2", "F7", false },
    { "SaveSlot3", "Save slot 3", "F8", false },
    { "LoadSlot1", "Load slot 1 (Shift)", "Shift+F6", true },
    { "LoadSlot2", "Load slot 2 (Shift)", "Shift+F7", true },
    { "LoadSlot3", "Load slot 3 (Shift)", "Shift+F8", true },
    { "AttackMove", "Squad attack-move", "A", false },
    { "StanceHold", "Stance: hold", "H", false },
    { "StanceGuard", "Stance: guard", "G", false },
    { "Patrol", "Squad patrol", "V", false },
    { "Rally", "Rally placement", "R", false },
    { "SelectType", "Select all of type", "C", false },
    { "SelectFactories", "Select all factories", "F", false },
    { "SlowestSpeed", "Move at slowest speed", "B", false },
    { "AreaBuild", "Area-build mode", "Z", false },
    { "AreaRepair", "Area-repair mode", "E", false },
    { "AttackGround", "Attack-ground mode", "X", false },
    { "AutoRetreat", "Auto-retreat toggle", "T", false },
    { "JumpPing", "Jump to latest ping", "J", false },
    { "ReplayBack", "Replay: step back", "Left", false },
    { "ReplayFwd", "Replay: step forward", "Right", false },
    { "Back", "Back / deselect / cancel", "Esc", false },
    { "Halt", "Halt selected units", "Space", false },
};

inline int NumProbeHotkeyDefs()
{
    return static_cast<int>(sizeof(kProbeHotkeyDefs) / sizeof(kProbeHotkeyDefs[0]));
}

struct ProbeLoadSlot
{
    const char* label;
    const char* path;
    bool filled;
};

// In-match outcome for the Step-2 HUD probe.
enum class ProbeOutcome
{
    None,
    Victory,
    Defeat
};

enum class ProbeSelectionKind
{
    None,
    Unit,
    Multi,
    Building
};

// Buildable rows mirror ProductionMenuOrder() in Hud.cpp with CostOf()
// values from src/game/private/units/UnitFactory.cpp:
// Infantry 25/0, Anti-Armor 30/5, Engineer 40/0, IFV 80/20,
// Artillery 120/40, Light Tank 150/50, Heavy Tank 250/100.
struct ProbeFactoryRow
{
    const char* name;
    long iron;
    long oil;
    bool repeat = false;
};

// Hint list mirrors ShortcutHintLines() in Hud.cpp with default hotkeys.
inline std::vector<std::string> ProbeHintLines()
{
    return {
        "WASD Camera",
        "Left Select",
        "Right Order",
        "Esc Deselect",
        "Space Halt",
        "P Pause",
        "F1 Hints",
        "F5 Save",
        "F9 Load",
        "A AttackMove",
        "H Hold",
        "G Guard",
        "V Patrol",
        "R Rally",
        "C SelectType",
        "F Factories",
        "B SlowMove",
        "Z Place",
        "Alt+RDrag Line",
        "X AttackGnd",
        "T AutoRetreat",
        "J JumpPing",
        "Shift Queues",
        "Wheel Zoom",
        "F6/F7/F8 SaveSlot",
        "Shift+F6/F7/F8 Load",
        "Ctrl+1-0 Group",
        "Shift+1-0 AddGrp",
        "1-0 Recall",
        "C+S+1-0 AutoAdd",
    };
}

struct ProbeState
{
    ProbePage page = ProbePage::MainMenu;
    ProbeConfirm confirm = ProbeConfirm::None;
    bool quitRequested = false;

    // MenuSettings equivalents with the real defaults.
    float cameraSpeed = 400.0f;
    bool showMinimap = true;
    bool rightDragPan = false;
    bool colorBlindMode = false;
    float uiScale = 1.0f;
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 1.0f;
    bool mute = false;

    // Skirmish setup fixtures (name/author/dims/path mirror data/maps/*.map).
    std::vector<ProbeMapEntry> maps;
    int mapIndex = -1; // nothing selected: Start is disabled
    ProbeDifficulty difficulty = ProbeDifficulty::Medium;

    ProbeLoadSlot slots[4] = {
        { "Quicksave", "data/quicksave.ccpb", true },
        { "Slot 1", "data/slot1.ccpb", true },
        { "Slot 2", "data/slot2.ccpb", false },
        { "Slot 3", "data/slot3.ccpb", false },
    };

    // ---- In-match mock state (Step 2 HUD probe, pure logic, no RmlUi) ----
    long iron = 500;
    long oil = 250;

    ProbeSelectionKind selKind = ProbeSelectionKind::Unit;
    const char* selType = "Light Tank";
    float selHp = 238.0f;
    float selMaxHp = 300.0f; // BaseStats(LightTank).health
    int selTeam = 0;
    int selCount = 1;
    const char* selBuildingType = "Factory";
    int selBuildingCount = 1;

    int idleWorkers = 2;
    int idleArmy = 3;

    bool playerAutoRepair = false;
    float autoRepairCap = 0.5f;

    int groupCounts[10] = { 4, 0, 2, 0, 0, 1, 0, 0, 0, 0 };
    bool groupAllSelected[10] = { true, false, false, false, false, true, false, false, false,
        false };
    int autoAddGroupBit = 2;

    bool hasFactory = true;
    ProbeFactoryRow factoryRows[7] = {
        { "Infantry", 25, 0 },
        { "Anti-Armor", 30, 5 },
        { "Engineer", 40, 0 },
        { "IFV", 80, 20 },
        { "Artillery", 120, 40 },
        { "Light Tank", 150, 50 },
        { "Heavy Tank", 250, 100 },
    };
    std::vector<int> prodQueue = { 0, 5 };
    float headProgress = 0.35f;

    const char* enemyDifficulty = "Medium";
    int wavesLaunched = 3;

    bool showHints = true;
    ProbeOutcome outcome = ProbeOutcome::None;

    void ResetMatch()
    {
        iron = 500;
        oil = 250;
        selKind = ProbeSelectionKind::Unit;
        selType = "Light Tank";
        selHp = 238.0f;
        selMaxHp = 300.0f;
        selTeam = 0;
        selCount = 1;
        selBuildingType = "Factory";
        selBuildingCount = 1;
        idleWorkers = 2;
        idleArmy = 3;
        playerAutoRepair = false;
        autoRepairCap = 0.5f;
        const int counts[10] = { 4, 0, 2, 0, 0, 1, 0, 0, 0, 0 };
        const bool allSel[10] = { true, false, false, false, false, true, false, false, false,
            false };
        for (int i = 0; i < 10; ++i)
        {
            groupCounts[i] = counts[i];
            groupAllSelected[i] = allSel[i];
        }
        autoAddGroupBit = 2;
        hasFactory = true;
        for (ProbeFactoryRow& row : factoryRows)
            row.repeat = false;
        prodQueue = { 0, 5 };
        headProgress = 0.35f;
        enemyDifficulty = "Medium";
        wavesLaunched = 3;
        showHints = true;
        outcome = ProbeOutcome::None;
    }

    int QueueSize() const { return static_cast<int>(prodQueue.size()); }

    std::string ResourceLine() const
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Iron: %ld  Oil: %ld", iron, oil);
        return buf;
    }

    std::string SelectionLine() const
    {
        char buf[128];
        switch (selKind)
        {
        case ProbeSelectionKind::Unit:
            std::snprintf(buf, sizeof(buf), "%s  HP %.0f/%.0f  Team %d", selType,
                static_cast<double>(selHp), static_cast<double>(selMaxHp), selTeam);
            return buf;
        case ProbeSelectionKind::Multi:
            std::snprintf(buf, sizeof(buf), "%d units selected", selCount);
            return buf;
        case ProbeSelectionKind::Building:
            std::snprintf(buf, sizeof(buf), "%d %s selected", selBuildingCount,
                selBuildingType);
            return buf;
        case ProbeSelectionKind::None:
        default:
            return "No selection";
        }
    }

    // Mirrors DrawSaveSlots(): "1+/- 2+/- 3+/-  (F6-8 save, S+F6-8 load)".
    std::string HudSlotsLine() const
    {
        char marks[3][8];
        for (int i = 0; i < 3; ++i)
        {
            const bool filled = slots[i + 1].filled;
            std::snprintf(marks[i], sizeof(marks[i]), "%d%s", i + 1, filled ? "+" : "-");
        }
        char line[96];
        std::snprintf(line, sizeof(line), "%s %s %s  (F6-8 save, S+F6-8 load)", marks[0],
            marks[1], marks[2]);
        return line;
    }

    std::string GroupLabel(int bit) const
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d:%d", (bit + 1) % 10, groupCounts[bit]);
        return buf;
    }

    bool CanAfford(int row) const
    {
        if (row < 0 || row >= 7 || !hasFactory)
            return false;
        return iron >= factoryRows[row].iron && oil >= factoryRows[row].oil;
    }

    bool Enqueue(int row)
    {
        if (!CanAfford(row))
            return false;
        iron -= factoryRows[row].iron;
        oil -= factoryRows[row].oil;
        prodQueue.push_back(row);
        return true;
    }

    void CancelTop()
    {
        if (prodQueue.empty())
            return;
        const int top = prodQueue.front();
        iron += factoryRows[top].iron;
        oil += factoryRows[top].oil;
        prodQueue.erase(prodQueue.begin());
        if (prodQueue.empty())
            headProgress = 0.0f;
    }

    void ToggleRepeat(int row)
    {
        if (row >= 0 && row < 7)
            factoryRows[row].repeat = !factoryRows[row].repeat;
    }

    void ToggleHints() { showHints = !showHints; }

    void SetOutcome(ProbeOutcome value) { outcome = value; }

    void SelectIdle(bool workers)
    {
        const int count = workers ? idleWorkers : idleArmy;
        if (count <= 0)
            return;
        selKind = ProbeSelectionKind::Multi;
        selCount = count;
    }

    ProbeState() { ResetMockMaps(); }

    void ResetMockMaps()
    {
        maps = {
            { "Crossroads", "dev-team", "data/maps/crossroads.map", 24, 18 },
            { "High Ridge", "dev-team", "data/maps/high_ridge.map", 24, 18 },
            { "Prototype Sandbox", "dev-team", "data/maps/prototype.map", 20, 10 },
            { "Salt Flats", "dev-team", "data/maps/salt_flats.map", 24, 18 },
            { "Twin Basins", "dev-team", "data/maps/twin_basins.map", 24, 18 },
            { "Twin Falls 2v2", "dev-team", "data/maps/twin_falls_2v2.map", 28, 20 },
        };
        mapIndex = -1;
        difficulty = ProbeDifficulty::Medium;
    }

    bool CanStart() const
    {
        return mapIndex >= 0 && mapIndex < static_cast<int>(maps.size());
    }

    const ProbeMapEntry* SelectedMap() const
    {
        return CanStart() ? &maps[static_cast<std::size_t>(mapIndex)] : nullptr;
    }

    bool ConfirmOpen() const { return confirm != ProbeConfirm::None; }
    void OpenQuitConfirm() { confirm = ProbeConfirm::QuitApp; }
    void OpenBackToMenuConfirm() { confirm = ProbeConfirm::BackToMenu; }
    void CloseConfirm() { confirm = ProbeConfirm::None; }

    void OpenMainMenu() { page = ProbePage::MainMenu; }
    void OpenSetup()
    {
        ResetMockMaps();
        page = ProbePage::SkirmishSetup;
    }
    void OpenSettings() { page = ProbePage::Settings; }
    void OpenHotkeyRemap() { page = ProbePage::HotkeyRemap; }
    void OpenLoad() { page = ProbePage::LoadGame; }

    void SelectMap(int index)
    {
        if (index >= 0 && index < static_cast<int>(maps.size()))
            mapIndex = index;
    }

    void SelectDifficulty(ProbeDifficulty value) { difficulty = value; }

    // SkirmishSetup -> Playing when CanStart; otherwise false (stays put).
    bool StartMatch()
    {
        if (page != ProbePage::SkirmishSetup || !CanStart())
            return false;
        ResetMatch();
        page = ProbePage::Playing;
        return true;
    }

    // Filled load slot -> Playing with fresh mock match state.
    bool StartFromSave(int slot)
    {
        if (slot < 0 || slot >= 4 || !slots[slot].filled)
            return false;
        ResetMatch();
        page = ProbePage::Playing;
        return true;
    }

    void TogglePause()
    {
        if (page == ProbePage::Playing)
            page = ProbePage::Paused;
        else if (page == ProbePage::Paused)
            page = ProbePage::Playing;
    }

    // Esc/Back policy, mirroring MenuFlow::OnBackPressed.
    ProbeBackAction OnBackPressed()
    {
        if (ConfirmOpen())
        {
            CloseConfirm();
            return ProbeBackAction::None;
        }
        switch (page)
        {
        case ProbePage::MainMenu:
            OpenQuitConfirm();
            return ProbeBackAction::OpenQuitConfirm;
        case ProbePage::SkirmishSetup:
        case ProbePage::Settings:
        case ProbePage::LoadGame:
            OpenMainMenu();
            return ProbeBackAction::Navigate;
        case ProbePage::Playing:
        case ProbePage::Paused:
            OpenBackToMenuConfirm();
            return ProbeBackAction::OpenBackToMenuConfirm;
        case ProbePage::GameOver:
        case ProbePage::Victory:
            return ProbeBackAction::QuitToMenu;
        case ProbePage::HotkeyRemap:
        default:
            return ProbeBackAction::None;
        }
    }

    // Confirm "Yes" side effects (the host owns these in the real game).
    void ResolveConfirmYes()
    {
        if (confirm == ProbeConfirm::QuitApp)
            quitRequested = true;
        else if (confirm == ProbeConfirm::BackToMenu)
            OpenMainMenu();
        CloseConfirm();
    }
};
