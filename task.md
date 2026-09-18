# Task: Reduce `src/` via raylib-extras / raysan5 third-party APIs

Execution log for `plans/ReduceSrcThirdPartyLibs_Plan.md` (6 phases, in order,
commit after each). Standing constraint: no multiplayer/networking work.

## Phase 1 — `raysan5/rini` for `data/settings.cfg`: NO-GO (reverted)

Attempted: vendored `deps/rini`, added `src/thirdparty/rini_impl.cpp`
(`RINI_VALUE_DELIMITER '='` + `RINI_IMPLEMENTATION`), rewrote
`Save/LoadSettings` (`Menu.cpp`) over rini, wired includes into
game/test/e2e premake projects.

Failures found (verified, not speculative):
1. rini with `'='` delimiter cannot parse unspaced `key=value` — its
   value scanner only stops at `' '`, overshoots the `'='`, and yields an
   empty value. Proven with a compiled probe against the vendored header:
   `mute=1` -> `text=[]`, `cameraSpeed=512.5` -> `text=[]`,
   `spaced = 42` -> `text=[42]`. Every existing user `settings.cfg`
   (written unspaced by all shipped versions) would load as defaults,
   silently dropping hotkey remaps — violating the plan's migration rule.
2. `menu_tests` then aborts (`0x80000003`, vector OOB at
   `hotkeyOverrides[1]`) because the remaps never load.
3. Side warts: `rini_save` emits padded/quoted format unreadable by old
   game versions; `rini.h` doesn't compile as C under MSVC (`bool`
   without `<stdbool.h>`); no save-error / missing-file signals
   (needed `ifstream` probes anyway); first-match getters vs our
   last-line-wins semantics (worked around, but more glue).

Net: ~60% of the custom logic (float/bool validators, hotkey loop,
probes) would stay anyway. Reverted everything; `--filter=Menu` back to
green (83 checks, 0 failures). Settings stay hand-rolled (~150 LOC,
well-tested, format-stable). Commit: this file (no src/ change).

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

## Phase 4 — `rres` packaging: (pending)

## Phase 5 — `raytilemap` migration: (pending)

## Phase 6 — `rlImGui` editor-only: (pending)
