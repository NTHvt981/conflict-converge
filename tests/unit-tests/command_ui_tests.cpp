// Unit tests for command UI: zoom limits, minimap inverse, shift
// chords, production menu order, save-slot paths, input snapshot fields.

#include "test_harness.h"

#include "app/ui/GameCamera.h"
#include "app/ui/Hud.h" // ProductionMenuOrder
#include "app/input/InputManager.h"
#include "app/ui/Minimap.h"
#include "app/save/SaveGame.h" // SaveSlotPath
#include "app/input/Shortcuts.h"

void RunCommandUiTests()
{
    // --- zoom clamps to [0.5, 2.0] ---
    {
        GameCamera camera;
        camera.view.zoom = 1.0f;
        camera.AdjustZoom(0.0f);
        CC_CHECK(camera.view.zoom == 1.0f);
        camera.AdjustZoom(1.0f);
        CC_CHECK(CcNear(camera.view.zoom, 1.125f));
        camera.AdjustZoom(100.0f);
        CC_CHECK(camera.view.zoom == 2.0f);
        camera.AdjustZoom(-100.0f);
        CC_CHECK(camera.view.zoom == 0.5f);
        GameCamera fresh; // zero zoom recovers to 1.0 before scaling
        fresh.AdjustZoom(1.0f);
        CC_CHECK(CcNear(fresh.view.zoom, 1.125f));
    }

    // --- minimap inverse roundtrips, clamps, contains ---
    // (screenRect assigned directly: Init/Unload need a GPU window.)
    {
        Minimap minimap;
        minimap.screenRect = { 630.0f, 320.0f, 160.0f, 120.0f };
        const Vector2 world{ 768.0f, 576.0f };
        const Vector2 back = minimap.MinimapToWorld(minimap.WorldToMinimap(world, 24, 18), 24, 18);
        CC_CHECK(CcNear(back.x, world.x, 1.0f) && CcNear(back.y, world.y, 1.0f));
        const Vector2 clamped = minimap.MinimapToWorld({ 0.0f, 0.0f }, 24, 18);
        CC_CHECK(clamped.x == 0.0f && clamped.y == 0.0f);
        const Vector2 far = minimap.MinimapToWorld({ 9999.0f, 9999.0f }, 24, 18);
        CC_CHECK(far.x == 24.0f * 64.0f && far.y == 18.0f * 64.0f);
        CC_CHECK(minimap.Contains({ 630.0f, 320.0f })); // edges inclusive
        CC_CHECK(minimap.Contains({ 700.0f, 380.0f }));
        CC_CHECK(!minimap.Contains({ 629.0f, 380.0f }));
        CC_CHECK(!minimap.Contains({ 700.0f, 441.0f }));
        const Vector2 degenerate = minimap.MinimapToWorld({ 700.0f, 380.0f }, 0, 18);
        CC_CHECK(degenerate.x == 0.0f && degenerate.y == 0.0f);
    }

    // --- shift chords route independently from plain bindings ---
    {
        ShortcutRegistry shortcuts;
        int plain = 0, chord = 0;
        shortcuts.Bind(10, [&] { ++plain; });
        shortcuts.BindChord(11, [&] { ++chord; });
        CC_CHECK(shortcuts.Fire(10));
        CC_CHECK(plain == 1);
        CC_CHECK(!shortcuts.Fire(11)); // chord key ignores plain Fire
        CC_CHECK(!shortcuts.FireChord(10)); // plain key ignores chord Fire
        CC_CHECK(shortcuts.FireChord(11));
        CC_CHECK(chord == 1);
        shortcuts.Unbind(11);
        CC_CHECK(!shortcuts.FireChord(11));
    }

    // --- production menu covers all 8 buildables exactly once ---
    // (PrototypeInfantry is sandbox-only, never in factory menus.)
    {
        const std::vector<UnitType> order = ProductionMenuOrder();
        CC_CHECK(order.size() == 8);
        bool seen[static_cast<int>(UnitType::Count)] = {};
        for (UnitType type : order)
        {
            const int i = static_cast<int>(type);
            CC_CHECK(i >= 0 && i < static_cast<int>(UnitType::Count) && !seen[i]);
            seen[i] = true;
        }
        CC_CHECK(!seen[static_cast<int>(UnitType::PrototypeInfantry)]);
    }

    // --- save slots map to data/ paths with clamping ---
    {
        CC_CHECK(SaveSlotPath(1) == "data/slot1.ccpb");
        CC_CHECK(SaveSlotPath(2) == "data/slot2.ccpb");
        CC_CHECK(SaveSlotPath(3) == "data/slot3.ccpb");
        CC_CHECK(SaveSlotPath(0) == "data/slot1.ccpb");
        CC_CHECK(SaveSlotPath(99) == "data/slot3.ccpb");
    }

    // --- input snapshot carries wheel + shift (defaults zeroed) ---
    {
        InputManager input;
        CC_CHECK(input.WheelDelta() == 0.0f && !input.ShiftDown());
        input.Snapshot({ 1.0f, 2.0f }, false, false, 2.0f, true);
        CC_CHECK(input.WheelDelta() == 2.0f && input.ShiftDown());
        input.Snapshot({ 1.0f, 2.0f }, true, false);
        CC_CHECK(input.WheelDelta() == 0.0f && !input.ShiftDown());
        CC_CHECK(input.LeftPressed() && !input.RightPressed());
    }
}
