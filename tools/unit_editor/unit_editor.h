#pragma once

#include "UnitConfig.h"

// Standalone unit-config editor state. The UI edits configs[type]
// in place; text-box buffers and dropdown actives are re-synced whenever
// the selection changes. No game-loop code lives here.
struct EditorState
{
    static constexpr int kTypeCount = 8;
    static constexpr int kTabStats = 0;
    static constexpr int kTabSprites = 1;
    static constexpr int kTabCollision = 2;

    UnitConfig configs[kTypeCount];
    bool fromFile[kTypeCount] = {};
    int selected = 0;
    int tab = kTabStats;

    // Sprite-tab text buffers (synced from configs[selected] on select).
    char spritePrefix[64] = {};
    char atlasIdle[128] = {};
    char atlasWalk[128] = {};
    bool spriteEdit = false;
    bool idleEdit = false;
    bool walkEdit = false;

    // Dropdown / checkbox UI state.
    int armorActive = 0;
    int damageActive = 0;
    bool armorEdit = false;
    bool damageEdit = false;
    int attackPowerBox = 0;
    int attackRangeBox = 0;
    bool attackPowerEdit = false;
    bool attackRangeEdit = false;
    // Collision-tab boxes: bounds L/T/R/B + origin X/Y (footprint derives
    // as R-L x B-T; see the schema doc in proto/unitconfig.proto).
    int boundLBox = 0;
    int boundTBox = 0;
    int boundRBox = 1;
    int boundBBox = 1;
    int originXBox = 0;
    int originYBox = 0;
    bool boundLEdit = false;
    bool boundTEdit = false;
    bool boundREdit = false;
    bool boundBEdit = false;
    bool originXEdit = false;
    bool originYEdit = false;
    bool animatePreview = true;
    int previewFacing = 6; // sheet column previewed (0-7, 6 = right)
    bool previewFacingEdit = false;
    int listScroll = 0;

    char status[128] = {};
};

// Loads data/configs (missing types fall back to DefaultUnitConfig) and
// syncs the edit buffers to configs[0].
void InitEditor(EditorState &editor);

// One frame of the editor window. `art` must be Init(true)'d + LoadAtlas'd
// by the caller (main.cpp); Save refreshes the atlas via LoadAtlas.
void DrawEditorFrame(EditorState &editor, class Art &art);
