# Conflict Converge

[![CI](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml/badge.svg)](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml)

A 2D real-time strategy demo built with [raylib](https://www.raylib.com/) (+ glm, raygui, cereal).
Tile-grid movement and A\* orders, 7 unit types, damage-matrix combat, iron/oil economy with
bases and production queues, fog of war, map files, an AI commander (Easy/Medium/Hard), synthesized
audio, sprite art with a rectangle fallback, and a boot-to-menu shell with skirmish setup,
settings persistence, and save slots. All milestones M1–M15 are complete — see
[`docs/history/milestones.md`](docs/history/milestones.md) and [`docs/history/decisions.md`](docs/history/decisions.md).

## Prerequisites

- Visual Studio 2022 (Community or newer) with C++ desktop workload
- MSVC toolset 14.44 or newer. `premake5 vs2022` writes `prj/Directory.Build.props`
  to select the newest installed v143 toolset, because an install defaults to an
  older one; the C++23 build fails on 14.36.
- [premake5](https://premake.github.io/) on `PATH`
- Python 3 (for `bootstrap.py`)

## Setup

```bat
python bootstrap.py
```

```bat
premake5 vs2022
```

Re-run `premake5 vs2022` after adding or removing any source file. Serialization
is header-only (cereal) — there is no codegen step.

## Build

The solution is **x64 only**:

```bat
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
msbuild prj/ConflictConverge.sln /p:Configuration=Release /p:Platform=x64
```

Compilation runs across all cores (`/MP`) and every game/test/e2e translation unit
shares one precompiled header (`src/game/public/core/CcPch.h`). Add a widely used,
rarely edited third-party header there instead of to individual sources; a source
that must `#define` something before such a header needs that define moved to the
project, because the PCH is force-included first.

## Test

```bat
prj/bin/Debug/conflict-converge-test.exe
prj/bin/Release/conflict-converge-test.exe
```

Exit code 1 on failure. Allow 60s+: the AI soak and 200-unit perf tests take seconds.

## Play

Launch `prj/bin/Release/conflict-converge.exe`. From the title screen pick
**Start Skirmish** (map + difficulty), **Load Game**, or **Settings**.

In-match keys: WASD pan, wheel zoom, left-click select (drag for box, Shift extends),
right-click order, A attack-move, H/G stances, V patrol, R rally mode, Space halt,
Esc deselect, P pause, F1 hints, F5/F9 quicksave/load, F6–F8 slots (Shift loads).

## Playtest

[`docs/playtest/`](docs/playtest/README.md) holds the note protocol, the template,
verified click recipes, and the index. Each note records observations separately
from fixes and ends in one keep, retune, or kill decision. An agent drives the
running build through the `windows-agent-mcp` window capture and input tools; the
skill that encodes the loop lives in `.opencode/skills/playtest/`.
