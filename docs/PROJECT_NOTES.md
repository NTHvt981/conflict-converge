# Use premake instead of cmake
Do not use CMake to build or generate project files
Use premake to generate project files, then run msbuild to build project

## Cereal migration Phase 3 done (2026-09-19)
`UnitConfig.cpp` ported; UnitConfig suite 82/82, full runner 4262 green.
Presence via `std::optional` + tolerant `load()` (explicit 0 survives, as
the tests demand); dual-casing fallback (`attackPower`→`attack_power`, …)
preserves protobuf-JSON's exact input acceptance — audited: shipped files
are all camelCase, fallback is for wild hand-edits. Absent→default,
mistyped→whole-file-reject (matches protobuf). No test additions needed:
existing shipped-seed + all-omitted cases already cover the plan's items.
New rule (bit twice now): cereal JSON wraps an UNNAMED root struct in a
`"value0"` object on output (and misaligns input) — both directions drive
named top-level members directly; nested structs are always named, fine.

## Cereal migration Phase 2 done (2026-09-19)
`SaveGame.cpp` ported to cereal binary; SaveGame suite 65/65, full runner
4262 green (+1: new old-CCPB-magic rejection test). Magic `CCPB`→`CCB2`,
`kSaveVersion` 1→2 (break-compat as decided). Wire structs live in
`src/game/private/app/SaveWire.h` (tests reach it via relative include,
same trick the old `savegame.pb.h` include used). Validation ported
line-for-line; H5 test now smuggles INT32_MAX through the real encode path
(hand-added registry Building) instead of a hand-built wire message.
Notes: binary archives need no tolerant-load dance and no assert override
(no rapidjson involved) — unnamed root struct is fine for binary; the
`cereal/types/*` includes are needed in EVERY TU that archives (test TU
hit the same C2338 until fixed).

## Cereal migration Phase 1 done (2026-09-19)
`SpriteData.cpp` ported to cereal JSON; suite 60/60, full runner 4261 green.
Two deviations from `plans/CerealMigration_Plan.md` Phase 1 (apply to Phases 2-3):
- Plain `serialize()` is insufficient: cereal throws on absent keys, but the
  schema/tests rely on protobuf default-instance semantics (omitted origin,
  mask ids, even whole top-level arrays). Every member loads via a
  `TryLoadValue` try/catch that keeps the default on absence.
- Never process an unnamed struct at a JSON root: cereal enters every `ar()`
  struct via `startNode()`, so a root struct silently misaligns the iterator
  stack (all lookups miss, empty sheet validates vacuously true). Load root
  members as top-level NVPs instead; nested structs are always named, fine.
- Fail-closed parsing needs the documented `CEREAL_RAPIDJSON_ASSERT` override
  (TU-local, before cereal includes): without it, mistyped scalars and
  malformed docs abort (Debug) instead of returning false like protobuf-JSON
  did. Also had to include `cereal/types/string.hpp` + `vector.hpp`
  explicitly (base `cereal.hpp` lacks container support — C2338 otherwise).

## Cereal migration step 0 decisions (2026-09-19)
- Go: migrate protobuf → cereal per `plans/CerealMigration_Plan.md`.
- Save compat: break it. Cereal binary layout can't read old `.ccpb` saves;
  bump magic + version (CCSV-rejection precedent). No back-compat shim.
- Facts confirmed: cereal latest tag is v1.3.2; `bootstrap.py:44` rmtree verified;
  `data/configs/*.json` all camelCase (no snake_case fallback needed);
  CI protobuf steps are `ci.yml:39-49`, README protobuf lines 5/17/26-28/35/39;
  suite baseline is 4261 checks.

## Pre-existing test failures (2026-09-19)
Found while verifying the include-comment cleanup (that change was comment-only,
so these fail without it too):
- `Skirmish` suite aborts (exit 3): `CC_ASSERT(InBounds(tile))` fires in
  `TileMap::Get` (`src/game/private/world/TileMap.cpp:63`) — FIXED 2026-09-19.
  Same stale-map family as the sandbox failure, but disguised as a crash:
  the test's BuildSandbox section called `sand.map.Get({5, 16})` (valid on
  the old 32x32 layout, OOB on the 20x10 map), so the assert fired inside the
  test itself — no game-code bug. Fixed like Phase A (dims 20x10) plus made
  the spawn-tile check rot-proof by reading `protoData.playerSpawns[0]`
  instead of a hardcoded tile. Full runner green: 4261 checks, 0 failures.
- `PrototypeSandboxMovement` (9 CHECK failures — and the earlier `Movement`
  EXIT=1, which was only the substring filter also running this suite; the
  Movement suite proper was always green): FIXED 2026-09-19. Root cause was
  stale expectations, not game code — commit `ccda23c` ("Scale down prototype
  map") shrank `data/maps/prototype.map` 32x32 → 20x10, orphaning the test's
  dims check and its 6 destinations (4 went OOB → `IsBlocked` true → units
  never arrived). Test now targets the 20x10 layout; suite green (50 checks).

## Logging research (2026-09-19)
- No game logging facility exists today: only `snprintf` formatting for HUD text
  and `CC_ASSERT` for debug checks. Nothing named `*Log*` in `src/`.
- Recommended: reuse raylib (already linked into every binary incl. headless
  tests) instead of adding a dependency.
  - `TraceLog(level, fmt, ...)` (`deps/raylib/src/raylib.h:1118`): printf-style,
    levels `LOG_TRACE/DEBUG/INFO/WARNING/ERROR/FATAL` (`raylib.h:575-582`).
    Desktop output is `vprintf` to stdout with a `LEVEL: ` prefix + flush
    (`rcore.c:1880`), default threshold `LOG_INFO` (`rcore.c:401`), adjustable
    via `SetTraceLogLevel` (`raylib.h:1117`). All premake projects are
    `ConsoleApp`, so stdout is visible without extra console setup.
  - File logging: `SetTraceLogCallback` (`raylib.h:1119`) with signature
    `void(int level, const char *text, va_list args)` (`raylib.h:966`).
    GOTCHA: installing a callback *replaces* stdout output (`rcore.c:1888-1893`
    returns early), so a file-logging callback must itself `vprintf` to keep
    console output. Suggested shape: `Log::Init(path)` opens `data/logs/…`
    (or beside the exe like quicksaves), installs a callback that formats with
    `vsnprintf` then writes to both file (flush per line, or on Shutdown) and
    stdout; `Log::Shutdown` restores `SetTraceLogCallback(NULL)` + closes.
  - Limits: single message cap `MAX_TRACELOG_MSG_LENGTH` 256 (`rcore.c:216`),
    longer text is truncated; `LOG_FATAL` calls `exit(EXIT_FAILURE)`.
  - Headless-safe: `TraceLog` needs no window/`InitWindow`, so it works in the
    test runner and e2e alike; keep per-frame hot-path logging out (or gate
    behind `LOG_DEBUG` + `SetTraceLogLevel`) to protect the 200-unit perf test.
- Implemented 2026-09-19: `src/game/public/app/Log.h` +
  `src/game/private/app/Log.cpp` (`Log::Init`/`Shutdown`/`Fatal`), wired in
  `Game::Init`/`Shutdown`, tested in `tests/unit-tests/log_tests.cpp` (Log
  suite green; `Fatal` exits so only Init/Shutdown + routing are covered).
