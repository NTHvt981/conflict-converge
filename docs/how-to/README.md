# How-To — Index

One recipe per extension activity. Steps name files, symbols, and
commands only — each doc is standalone, and a step that renders it
stale is a bug in the doc.

## Available

- [add-a-unit](add-a-unit.md) — ship a stat-driven unit: tripwired
  registration in `Unit.h` / `kUnitTypes` / `kTable`, the save-count
  review, optional config/AI/art, balance-sheet row.

## Verify

Every recipe lands in a repo that builds and passes its suite:

```bat
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
prj/bin/Debug/conflict-converge-test.exe
```

Exit code 1 on test failure. Release checks go through the same suite
with `Configuration=Release` when the change touches sim timing.
