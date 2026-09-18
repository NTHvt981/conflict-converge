# H6 Plan: Decompose `Game::Update` (Game.cpp:845-2563, ~1700 lines)

## Status: plan only, no code changed

Finding (v0.1 D1/CA-001, confirmed): one function interleaves menu screens,
input dispatch, simulation stepping, and world+HUD rendering. Contract for
every slice below: extract method(s) on `Game`, zero behavior change, full
suite green per slice, one commit per slice. Precedent: `DrawHotkeyRemap`,
`BindShortcuts`, `StepReplay` already live as extracted helpers.

## Measured structure (line numbers at audit commit)

- 847-1269: menu/no-world branch (per-screen render between its own
  Begin/EndDrawing).
- ~1270-1710: Playing input dispatch (drag-box, area-build/repair,
  line-formation, attack-ground, orders, control groups, hotkeys, camera).
- ~1710-1990: sim ticks (fog 1713, movement 1718, economy, production, AI,
  minimap texture 1908-1943, outcome 1949).
- 1992-2563: frame render (cursor, shake/camera, world 2050-2309,
  HUD/minimap/overlays to 2563).

## Slices (in order — first independently valuable)

1. `StepSimulation(dt)` ← ~1710-1990. Highest value: the only
   headless-testable slice (everything in it already runs without a
   window); extraction unlocks direct sim-step tests. Move verbatim,
   keep member access.
2. `DrawMenuBranch()` ← 847-1269. Self-contained per-screen dispatch;
   watch shared locals (`cx`, remap state) — hoist or pass explicitly.
3. `DrawWorld()` ← 2050-2309 and `DrawHudAndOverlays()` ← 2310-2563.
   Split at the existing EndMode2D boundary; no state crosses it except
   the shaken camera (parameter).
4. `DispatchPlayingInput()` ← ~1270-1710, then per-gesture helpers.
   Most intertwined (shared drag/mode state); do last, one gesture per
   commit if needed.

## Non-goals

No new files/TUs (methods on `Game` first; split TUs only if a slice
proves independently compilable), no behavior change, no renaming of
called helpers. e2e binary stays the wiring gate throughout.
