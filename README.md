# Conflict Converge

[![CI](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml/badge.svg)](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml)

A 2D real-time strategy demo built with [raylib](https://www.raylib.com/) (+ glm, raygui, protobuf).
Tile-grid movement and A\* orders, 7 unit types, damage-matrix combat, iron/oil economy with
bases and factory queues, fog of war, map files, an AI commander (Easy/Medium/Hard), synthesized
audio, sprite art with a rectangle fallback, and a boot-to-menu shell with skirmish setup,
settings persistence, and save slots. All milestones M1–M15 are complete — see
[`plans/MILESTONES.md`](plans/MILESTONES.md) and [`plans/Q_AND_A.md`](plans/Q_AND_A.md).

## Prerequisites

- Visual Studio 2022 (Community or newer) with C++ desktop workload
- [premake5](https://premake.github.io/) on `PATH`
- Python 3 (for `bootstrap.py`)
- CMake (the VS-bundled one works)

## Setup

```bat
python bootstrap.py
```

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S deps/protobuf/cmake -B deps/protobuf/build -G "Visual Studio 17 2022" -A Win32 -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_EXAMPLES=OFF -Dprotobuf_WITH_ZLIB=OFF -Dprotobuf_MSVC_STATIC_RUNTIME=OFF
cmake --build deps/protobuf/build --config Release --target libprotobuf protoc
cmake --build deps/protobuf/build --config Debug --target libprotobuf
```

```bat
premake5 vs2022
```

Re-run `premake5 vs2022` after adding or removing any source file. Re-run protoc codegen
after editing the schema (generated code is checked in, CI verifies it is current):

```bat
deps/protobuf/build/Release/protoc.exe --proto_path=proto --cpp_out=src/game/private/app proto/savegame.proto
```

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
