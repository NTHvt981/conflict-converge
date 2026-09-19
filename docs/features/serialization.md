# Serialization

Shipped: 2026-09-19 (Phases 0–4: commits `575dec5`/`22b888d`/`107e2f4`/
`c8abdd4`/`017de68`). Migrated protobuf → cereal; old `.ccpb` saves do
not load (magic bump, CCSV-rejection precedent).

All persistence (binary saves, JSON sprite/config data) runs on the
header-only cereal library: no CMake step, no codegen, no per-config
native libs.

## Behavior

- Binary saves: `SaveWire.h` structs ↔ `SaveGameData`, `CCB2` magic,
  version 2, two-phase validated load (target-index remap, fail-closed
  on garbage). Old `CCSV` and v1 `CCPB` files rejected at the magic
  check.
- Sprite atlas JSON: tolerant load (absent keys keep defaults, mistyped
  values reject the file), 4-file merge, same validation as before.
- Unit configs: `std::optional` presence tracking (explicit `0`
  survives), dual camelCase/snake_case key acceptance, same
  default-fallback merge onto compiled-in baselines.
- JSON archives never process an unnamed root struct (cereal wraps
  output in `"value0"` and misaligns input) — both directions drive
  named top-level members.

## Key files

- `src/game/private/app/SaveWire.h` — save wire structs.
- `src/game/private/app/SaveGame.cpp` — encode/decode + validation.
- `src/game/private/app/SpriteData.cpp` — atlas loader.
- `src/game/private/app/UnitConfig.cpp` — config loader/writer.
- `deps/cereal` (v1.3.2, via `libs_deps.json`) — the only serialization
  dependency.
- Tests: `SaveGame` (round-trip, adversarial, replay), `SpriteData`
  (real files, rejections), `UnitConfig` (round-trip, shipped files,
  tolerant loads) suites.

## Decisions

- Break save compat rather than shim (one-way door, confirmed upfront).
- Wire structs stay separate from live structs (independent versioning,
  validate-before-touch).
- `.ccpb` extension kept (magic carries format identity).
- See `PROJECT_NOTES.md` (Cereal migration entries) for the full
  deviation log.
