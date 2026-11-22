# Conflict Converge

[![CI](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml/badge.svg)](https://github.com/NTHvt981/conflict-converge/actions/workflows/ci.yml)

Conflict Converge is a 2D real-time strategy game for Windows. It builds on
[raylib](https://www.raylib.com/), with glm for math, [RmlUi](https://mikke89.github.io/RmlUiDoc/)
for menus and the HUD, raygui for the map editor, and cereal for saves.

The game has 10 unit types, 4 building types, and a damage matrix with 3 damage
types and 3 armor types. It also has an iron and oil economy, fog of war, text
map files, and an AI commander with 3 difficulties. It supports 2v2 skirmish
with an allied AI, and includes a replay viewer and synthesized audio. All
milestones M1–M16 are complete. See
[`docs/history/milestones.md`](docs/history/milestones.md) and
[`docs/history/decisions.md`](docs/history/decisions.md).

## Requirements

- Windows with Visual Studio 2022 (Community or newer) and the C++ desktop
  workload.
- MSVC toolset 14.44 or newer. `premake5 vs2022` writes
  `prj/Directory.Build.props` to select the newest installed v143 toolset,
  because an install can default to an older one. The C++23 build fails on
  14.36.
- [premake5](https://premake.github.io/) on `PATH`.
- Python 3 for `bootstrap.py`.

## Setup

1. Run `python bootstrap.py`. This clones the dependencies in
   `libs_deps.json` into `deps/`.
2. Run `premake5 vs2022`.

Run `premake5 vs2022` again after you add or remove a source file. Serialization
is header-only (cereal), so there is no codegen step.

## Build

The solution builds x64 only:

```bat
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
msbuild prj/ConflictConverge.sln /p:Configuration=Release /p:Platform=x64
```

Every game, test, and e2e translation unit shares one precompiled header
(`src/game/public/core/CcPch.h`). Add a widely used, rarely edited third-party
header there, not to individual sources. A source that must `#define` something
before such a header needs that define moved to the project file, because the
compiler force-includes the PCH first.

## Test

```bat
prj/bin/Debug/conflict-converge-test.exe
prj/bin/Release/conflict-converge-test.exe
```

The runner exits with code 1 on failure. Allow 60 seconds or more on a cold
run.

- `--list` prints the suite names.
- `--filter=Name` runs only the suites that contain the substring. Repeat the
  flag to run more than one suite.

The end-to-end harness drives the real game loop in a hidden window. It needs a
GPU, and you must start it from the repo root:

```bat
prj/bin/Debug/conflict-converge-e2e.exe
```

For an AddressSanitizer build, run the command below, then start
`prj/bin/ASan/conflict-converge-test.exe` from the repo root with the MSVC x64
bin directory on `PATH`:

```bat
msbuild prj/conflict-converge-test.vcxproj /p:Configuration=ASan /p:Platform=x64
```

## Play

Start `prj/bin/Release/conflict-converge.exe`. The main menu has these entries:

- **Start Skirmish** picks a map and an AI difficulty (Easy, Medium, Hard).
  `data/maps/twin_falls_2v2.map` starts a 2v2 match with an allied AI.
- **Load Game** loads a save slot or plays the last replay.
- **Settings** sets audio, camera, display, and hotkey options.
- **Map Editor** paints a map and saves it to `data/maps/`.
- **Quit** closes the game.

In-match controls:

| Action | Keys |
|---|---|
| Pan and zoom | WASD, mouse wheel |
| Select | Left-click, drag a box, Shift extends |
| Select all of type / all production | C / F |
| Control groups | Ctrl+1–0 assigns, Shift+1–0 adds, 1–0 recalls |
| Order | Right-click, Shift queues the order |
| Attack-move / patrol / rally / halt | A / V / R / Space |
| Stance hold / guard | H / G |
| Auto-retreat | T |
| Area build / area repair / attack ground / slowest speed | Z / E / X / B |
| Deselect or cancel | Esc |
| Pause / shortcut hints | P / F1 |
| Quicksave / quickload | F5 / F9 |
| Save slot / load slot | F6–F8 / Shift+F6–F8 |
| Jump to latest ping | J |
| Replay step | Left / Right |

Ctrl+Shift+1–0 adds new units to a group as they spawn.

## Tools

`prj/bin/Release/conflict-converge-editor.exe` is a standalone unit-config
editor. It edits the JSON files in `data/configs/` and previews the sprite. The
game loads these files at match start.

## Project layout

```
src/game/public, src/game/private  game code by subsystem (core, world, units,
                                   economy, app/ui, app/input, app/save,
                                   app/data, app/match)
tests/                             unit, integration, and e2e harnesses
data/                              maps, configs, sprites, audio, and UI files
tools/                             content pipeline and the unit editor
docs/                              module map, feature notes, how-to, playtest
```

## Documentation

| File | Holds |
|---|---|
| [`docs/MODULE_MAP.md`](docs/MODULE_MAP.md) | Per-header responsibilities and milestone tags |
| [`docs/features/`](docs/features/) | One note per shipped feature |
| [`docs/how-to/`](docs/how-to/README.md) | Recipes for extending the game |
| [`docs/BALANCE.md`](docs/BALANCE.md) | Cost, health, damage, and AI wave tables |
| [`docs/conventions/`](docs/conventions/) | Coding and git rules |
| [`docs/history/`](docs/history/milestones.md) | Milestones and design decisions |

## Extending

Recipes for extending the game live in [`docs/how-to/`](docs/how-to/README.md).
Each recipe is standalone and verified against a build.

## Playtest

[`docs/playtest/`](docs/playtest/README.md) holds the note protocol, the
template, verified click recipes, and the index. Each note records observations
separately from suggested changes and ends in one keep, retune, or kill
decision. An agent drives the running build through the `windows-agent-mcp`
window capture and input tools. The skill that encodes the loop lives in
`.opencode/skills/playtest/`.
