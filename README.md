# Conflict Converge

[![CI](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml/badge.svg)](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml)

A 2D real-time strategy demo built with [raylib](https://www.raylib.com/) (+ glm, raygui, cereal).
Tile-grid movement and A\* orders, 7 unit types, damage-matrix combat, iron/oil economy with
bases and factory queues, fog of war, map files, an AI commander (Easy/Medium/Hard), synthesized
audio, sprite art with a rectangle fallback, and a boot-to-menu shell with skirmish setup,
settings persistence, and save slots. All milestones M1–M15 are complete — see
[`docs/history/milestones.md`](docs/history/milestones.md) and [`docs/history/decisions.md`](docs/history/decisions.md).

## Prerequisites

- Visual Studio 2022 (Community or newer) with C++ desktop workload
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

The solution is **Win32 only** (x64 fails):

```bat
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=Win32
msbuild prj/ConflictConverge.sln /p:Configuration=Release /p:Platform=Win32
```

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
