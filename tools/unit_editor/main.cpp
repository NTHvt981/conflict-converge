// Standalone unit-config editor entry point (see
// plans/Standalone_Unit_Config_Editor_Plan.md section 2): its own windowed
// binary sharing only the data-layer + art TUs, never Game.cpp.
#include "raylib.h"
#include "raygui.h"

#include <cstdio> // init-failure status message

#include "Art.h"
#include "unit_editor.h"

int main()
{
    InitWindow(1000, 608, "Conflict Converge - Unit Config Editor");
    SetTargetFPS(60);

    Art art;
    const bool artReady = art.Init(true);
    art.LoadAtlas();

    EditorState editor;
    InitEditor(editor);
    if (!artReady)
    {
        std::snprintf(editor.status, sizeof(editor.status),
                      "Art failed to init - previews unavailable");
    }

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
        DrawEditorFrame(editor, art);
        EndDrawing();
    }

    art.Shutdown();
    CloseWindow();
    return 0;
}
