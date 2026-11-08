# How-To — Index

One recipe per extension activity. Steps name files, symbols, and
commands only — each doc is standalone, and a step that renders it
stale is a bug in the doc.

## Units

- [add-a-unit](add-a-unit.md) — ship a stat-driven unit: tripwired
  registration in `Unit.h` / `kUnitTypes` / `kTable`, the save-count
  review, optional config/AI/art, balance-sheet row.
- [add-unit-behavior](add-unit-behavior.md) — teach units a genuinely
  new pattern: category → owning TU → in-tree exemplar, frame tick,
  headless test.

## Buildings & content

- [add-a-building](add-a-building.md) — ship a structure: type-info
  row, save-count review, Buildings-tab slot, optional art, balance
  row.
- [add-a-map](add-a-map.md) — author a `.map` against the format spec,
  register it in the shipped sanity loop, ship.

## Input & UI

- [add-a-hotkey](add-a-hotkey.md) — Tier-1 remappable action: one
  table row, suite count bump, guarded binding.
- [add-an-RmlUi-panel](add-an-RmlUi-panel.md) — a menu-branch
  document: ids in `UiIds.h`, load/own/show for a state, refresh and
  event wiring.

## Systems

- [extend-the-save-format](extend-the-save-format.md) — tolerant
  additive wire fields, enum validation, version bump rules.
- [write-a-test-suite](write-a-test-suite.md) — a headless suite:
  TU, registry row, premake regen, filter run.

## Verify

Every recipe lands in a repo that builds and passes its suite:

```bat
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
prj/bin/Debug/conflict-converge-test.exe
```

Exit code 1 on test failure. Release checks go through the same suite
with `Configuration=Release` when the change touches sim timing.
