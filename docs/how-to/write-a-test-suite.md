# Write a test suite

## Goal

Add a headless suite to the unit-test binary: plain logic checks with
no window, registered under a filterable name. Headless-first is the
repo standard — pure logic stays unit-testable without a window, raylib
draw calls live in render sections behind thin wrappers, and sim work
drives `Simulation::Step` directly.

Multi-system scenarios and the hidden-window loop live in separate
projects: `tests/integration-tests` holds the headless cross-system
suites, `tests/e2e` drives the real game loop on a GPU. Take a
scenario there when it stops being single-subsystem.

## Prerequisites

A Debug build that compiles and passes the test suite.

## Steps

1. **Create the translation unit.** `tests/unit-tests/<topic>_tests.cpp`
   with:

   ```cpp
   #include "test_harness.h"

   void Run<Suite>Tests()
   {
       // CC_CHECK(cond) on every invariant.
   }
   ```

   `CC_CHECK` counts and prints `FAIL` without aborting, so later
   checks still run; the binary exits 1 if any suite has failures.
   Use `CC_WARN` for trajectories that legitimately differ across
   machines (sim-outcome timing) — it warns instead of failing.

2. **Register it.** In `tests/main.cpp`, forward-declare
   `void Run<Suite>Tests();` and append
   `{ "<SuiteName>", Run<Suite>Tests }` to `kSuites`. The registry
   name is the filter substring — pick it as one word.

3. **Regenerate the project files.** New source files require:

   ```bat
   premake5 vs2022
   ```

4. **Run the suite in isolation.**

   ```bat
   prj/bin/Debug/conflict-converge-test.exe --filter=<SuiteName>
   ```

   Filters are repeatable unions. A bad filter exits 2.

5. **Honor the shared fixtures.** Suites construct their own
   registries and maps inline; nothing global is shared between
   suites. A suite needing `data/` assets resolves the repo-relative
   candidates the loaders already search (`data/…`, `../../data/…`,
   `../../../data/…`), because it must run from both the repo root and
   `prj/bin/<Config>`.
