# Add a unit

## Goal

Ship a new combat unit driving only its stat line: cost, health, armor,
damage, range, cooldown, speed, sight, footprint, and which production
tab it queues under. The unit comes out playable everywhere — player-buildable, fielded by
AI difficulty setups that list it, loadable in saves, queueable in
production, visible on the field — with zero art required to ship.

A unit whose behavior differs from an existing one (heals like the
Medic, transports like the IFV, arcs shells like the Artillery) is an
add-unit-behavior topic, beyond this recipe.

## Prerequisites

A Debug build that compiles and passes the test suite.

## Steps

1. **Declare the type.** In `src/game/public/units/Unit.h`, append one
   name to `enum class UnitType`, just above `Count`. Pick the enum
   name in PascalCase (`Sentinel`); nothing else is registered by
   string, so the other names below are chosen here.

2. **Add the type-info row.** In `src/game/public/units/UnitTypeInfo.h`,
   append one row to `kUnitTypes`. The `static_assert` under the table
   fails until the row count matches `UnitType::Count`, so the compiler
   catches the pairing. The row fields, left to right:

   | Field | Role |
   |---|---|
   | `displayName` | HUD and production-panel label |
   | `configName` | JSON `"type"` value and `ai.json` composition name |
   | `fileStem` | `data/configs/<fileStem>.json` override filename |
   | `spriteStem` | first part of flat-PNG filenames |
   | `artDir` | folder under `data/sprites/units/` |
   | `tankArtPrefix` | `nullptr` unless the unit uses a body/head atlas |
   | `atlasIdlePrefix`, `atlasMovePrefix` | `""` = flat-PNG or rectangle fallback |
   | `isVehicle` | `true` queues the unit on the Vehicle production tab and flips vehicle handling |
   | `Locomotor` | `Foot`, `Wheeled`, or `Tracked`, which selects pathing costs |
   | `ironCost`, `oilCost` | production price |
   | footprint | tiles, `{ width, height }` |

3. **Add the stats row.** In `src/game/private/units/UnitStats.cpp`,
   append one line to `kTable` and raise the count inside its
   `static_assert` to the new row count. Order: `health`, `armorType`,
   `damageType`, `attackPower`, `attackRange`, `cooldownTime`, `speed`,
   `sightRange`. A row with `attackPower` 0 is a support unit.

4. **Review the save tripwire.** In
   `src/game/private/app/save/SaveDecode.cpp`, the
   `static_assert(static_cast<int>(UnitType::Count) == ...)` fails on
   the first build after step 1. Confirm the new entry went to the **end**
   of the enum — appended entries keep every existing save's stored
   type values valid — then bump the expected count.

5. **Optionally, add a config override.** The defaults from steps 2–3
   are already live; the JSON file exists to override them. Copy the
   closest existing unit's file from `data/configs/`, rename it to
   `<fileStem>.json`, set `"type"` to `configName`, and delete the
   fields you want defaulted. The loader (`LoadAllUnitConfigs`) reads
   every file in that directory, and a file whose `"type"` matches no
   enum name is rejected with a logged warning.

6. **Optionally, teach the AI to build it.** In `data/configs/ai.json`,
   add `configName` to the `composition` arrays of the difficulties
   that should field it. Units absent from every composition are still
   player-buildable, just never fielded against the player.

7. **Optionally, add art.** Flat PNGs go to
   `data/sprites/units/<artDir>/<spriteStem>_<blue|red>_<idle|attack>.png`.
   Shipped with no files, the unit renders as a team-colored rectangle
   — the same fallback every flat-art unit uses for missing frames — so
   art is never a blocker. Atlas overrides (`atlasIdlePrefix`,
   `atlasMovePrefix`) belong to units with directional sprite sheets.

8. **Update the balance sheet.** Add the new unit's row to the Units
   table in `docs/BALANCE.md`, sourced from the numbers in steps 2–3.
