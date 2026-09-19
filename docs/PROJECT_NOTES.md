# Use premake instead of cmake
Do not use CMake to build or generate project files
Use premake to generate project files, then run msbuild to build project

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
