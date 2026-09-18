# Task: Reduce `src/` via raylib-extras / raysan5 third-party APIs

Execution log for `plans/ReduceSrcThirdPartyLibs_Plan.md` (6 phases, in order,
commit after each). Standing constraint: no multiplayer/networking work.

## Phase 1 — `raysan5/rini` for `data/settings.cfg`: SHIPPED (compat fixed)

First attempt failed: rini with `'='` delimiter only parses spaced pairs
— its value scanner stops at `' '`, so bare `key=value` (every shipped
settings.cfg) yielded empty values (proven with a compiled probe:
`mute=1` -> `text=[]`; menu tests then aborted on vector OOB). Reverted.

Fix: `NormalizeSettingsText` (`Menu.cpp`, ~25 lines) rewrites content
lines to `key = value` before `rini_load_from_memory`, so stock rini
(with `RINI_VALUE_DELIMITER '='` in the sole `RINI_IMPLEMENTATION` TU,
`src/thirdparty/rini_impl.cpp`) reads old files with no fork — survives
bootstrap re-clones. Kept verbatim: clamp ranges, `0`/`1`-only bools,
known-action + positive-int hotkeys with last-line-wins (via last-match
`FindEntryText` scan + in-order hotkey loop; rini's own getters return
first match), missing-file untouched, save-failure signal via probe
(rini reports no I/O status). New files save in rini's padded/quoted
shape (downgrade by old versions not preserved — upgrade only, per plan).

Wiring: `rini` entry in `libs_deps.json`, `deps/rini/src` includes in
game/test/e2e projects, `rini_impl.cpp` in test/e2e file lists (game
picks it up via `src/**` glob), `_CRT_SECURE_NO_WARNINGS` for that TU
only. Menu 83/83, full suite 4137/0. Net: `Menu.cpp` line-split loop
replaced by rini load/save; validators stay (they're the semantics).

## Phase 2 — `extras-c/path_utils`: NO CODE CHANGE (dead includes removed)

Confirmed against the vendored source (`deps/extras-c/path_utils/`):
`SearchAndSetResourceDir` chdirs INTO `data/` and takes no injectable
`dirExists`/`appDir` — it cannot replace `PickDataRoot`
(`DataRoot.h:19-37`, parent-of-`data` semantics + headless-testable
injected function pointer) without regressing every `data/`-prefixed
path and testability. `DataRoot.h` and the `Game.cpp:125-130` call site
stay untouched.

Cleanup: repo-wide grep shows nothing includes any `extras-c` header
(only `DataRoot.h` comment mentions), so the 4 dead
`deps/extras-c/path_utils` includedirs were removed from `premake5.lua`
(game/test/e2e/editor). `libs_deps.json` keeps the `extras-c` clone
(`ray_collision_2d.h`, cameras remain available). `DataRoot` tests green
(6 checks, 0 failures). Commit: `premake5.lua` + this file.

## Phase 3 — `reasings` sweep: NO ACTION (verified)

Repo-wide grep: `Ease*` used exactly once (`Menu.h:137` `EaseQuadOut`
fade); zero `Lerp`/`Vector2Lerp` calls in `src/` (other hits are
sprite-frame `animations` data and `between` comments). `Shake.h`
is continuous per-frame linear decay (`trauma -= rate*dt`), not a
fixed-duration tween — forcing it into `EaseXxx(t,b,c,d)` would change
feel, not reduce code. `GameCamera` zoom steps instantly (`*1.125`,
no smoothing to replace); `Pings` has no fade math. No source change.
Commit: this file.

## Phase 4 — `rres` packaging: DEFERRED (evaluated, no change)

Verified against the upstream repo (shallow clone): `rres.h` (reader,
C-clean with guarded `stdbool`) + `rres-raylib.h` (`LoadDataFromResource`/
`LoadImageFromResource`/`LoadWaveFromResource`/...) exist as documented,
and `examples/rres_create_file.c` gives an authoritative pack path. So
integration is possible — but declined for now, with reasons:
1. Wrong payoff shape for this goal: wiring adds ~50-80 LOC of loader
   glue + fallback branches to `Art::Init` (`Art.cpp:312-417`, ~40
   individual `LoadTexture` calls) and `Audio.cpp:93` to save 3
   near-identical `{COPYDIR}` postbuild lines. That GROWS `src/`.
2. Needs a pack step that doesn't exist in-repo (no `rrespacker`; options
   are a hand-rolled Python packer risking format drift, or building the
   C writer example as a one-off tool) — new tooling surface for a
   release-time optimization.
3. Cannot validate here: the packed path only runs in the device branch
   (`Init(true)`, GPU upload via `LoadTextureFromImage`), headless tests
   take the early-out (`Init(false)`), and this environment has no GPU
   context for the game/e2e binaries. Shipping GPU-only glue with zero
   runtime verification would be reckless; committing it inactive behind
   the loose-file fallback adds dead code (the thing Phase 2 just
   removed elsewhere).
Revisit when: release packaging hurts (many-file deploys) AND a GPU
 rig can validate the packed path. Loose `data/` + postbuild copy stays:
 tested, fast for dev iteration. Commit: this file.

## Phase 5 — `raytilemap` migration: NO-GO (measured, no change)

Measured `MapFile.cpp` (485 lines): the pure text parser
(`ParseDim`/`ParseValue`/`ParseMapFile`, ~135 lines) is the ONLY part
`raytilemap` (Tiled TMX tile-layer reader/renderer) could replace. The
remaining ~350 lines are format-agnostic game logic with no
third-party equivalent and stay regardless: `ApplyMapData`,
`WriteMapFile` (editor save), `ValidateMapPlayable` (reachability
check), `PaintEditorCell`, `NearestFreeTile`.
Worse, the `.map` format's real content is markers, not terrain:
`I`/`O` resource nodes, `1`/`2` spawns (incl. 2v2 twin maps),
`B` pre-placed buildings across 6 shipped maps — all needing custom
Tiled object-layer conventions + custom parsing, plus a `.map`→TMX
converter, dual-format transition, editor TMX-write support, and
`TerrainType` value preservation (`TileMap.h:19`, old saves decode
0-2). Net src/ change ≈ zero or positive, for real regression
surface across maps, saves, editor, and skirmish setup.
Per the plan's own gate (adopt only with Tiled commitment): declined.
Flips only if the team wants Tiled as a visual authoring tool — then
`raytilemap` slots in as tile renderer, markers/editor stay custom.
Commit: this file.

## Phase 6 — `rlImGui` editor-only: DECLINED (no requirement, additive)

Inspected `tools/unit_editor` (`main.cpp` 40 lines + `unit_editor.*`
~330 lines): a working raygui-based tool sharing only data-layer + art
TUs. The plan names no missing panel ("whatever inspector/debug panel
is wanted there first") — there is no requirement to implement.
Cost of proceeding anyway: vendor Dear ImGui (transitive dep, needs
its own static-lib premake project like `raylib-static`) + `rlImGui`
bridge + a second UI framework in the tool, all ADDITIVE. Nothing in
`src/` gets deleted; the in-game HUD (`Hud.cpp` raygui) is explicitly
out of scope per the plan itself. Marked low-priority/anytime there —
with no named feature and a reduce-src/ goal, the correct call is not
now. Revisit if the editor outgrows raygui (docking, complex
inspectors). Commit: this file.

## Bottom line

All 6 phases executed in order, one commit each. Net `src/` change:
-4 dead includedirs (`premake5.lua`); `src/` untouched otherwise.
Finding: the custom code (settings format + hotkey migration, `PickDataRoot`
semantics, marker-rich map format + editor, trauma-decay shake) is
load-bearing — the surveyed libs mismatch on format or semantics everywhere
it matters. Only `extras-c` cleanup survived contact with the code.
