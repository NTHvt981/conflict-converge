# Add a building

## Goal

Ship a new structure with a stat line: footprint, health, build time,
cost, and whether it produces units. The building comes out placeable
from the Buildings tab, ramping from placement to Operational over its
build time, loadable in saves — art optional.

A structure needing new behavior (a radar reveal pulse, a defense
turret) goes past this recipe; its logic belongs in the building code
paths, not a stat row.

## Prerequisites

A Debug build that compiles and passes the test suite.

## Steps

1. **Declare the type.** In `src/game/public/economy/Building.h`, append
   one name to `enum class BuildingType`, just above `Count`.

2. **Add the type-info row.** In `src/game/public/economy/BuildingTypeInfo.h`,
   append one row to `kBuildingTypes`; the `static_assert` under the
   table fails until the row count matches `BuildingType::Count`.
   Row fields, left to right:

   | Field | Role |
   |---|---|
   | `displayName` | Buildings-tab button label |
   | `flatStem` | flat-PNG name; `nullptr` = no flat art, masked art only |
   | `maskedStem` | masked-PNG name; `nullptr` = no masked art |
   | footprint | tiles, `{ width, height }` |
   | `maxHealth` | at Operational (constructor target) |
   | `buildTime` | seconds from placement to Operational |
   | `ironCost`, `oilCost` | placement price |
   | `isProducer` | `true` = hosts a production queue |
   | `producerCategory` | which `ProductionQueue` tab a producer serves |

3. **Review the save tripwire.** In
   `src/game/private/app/save/SaveDecode.cpp`, the
   `static_assert(static_cast<int>(BuildingType::Count) == ...)`
   fails on the first build after step 1. Confirm the entry sits at the
   **end** of the enum, then bump the expected count. Appended entries
   keep every existing save's stored values valid.

4. **Claim a Buildings-tab slot.** In `src/game/private/app/ui/Hud.cpp`,
   add the type to `BuildingMenuOrder()`. The tab shows the **first
   four** entries of that list — `hud.rml` owns exactly four placing
   rows (`PlaceRow0`–`PlaceRow3`) — so a fifth entry sits inert until
   another row exists in `data/ui/hud.rml`. Reorder deliberately.

5. **Optionally, add art.** Flat art goes to
   `data/sprites/buildings/<flatStem>_<blue|red>.png`, frame art
   `<flatStem>_<frame>_<blue|red>.png`; masked stem pairs live beside
   them. Missing files fall back to the team-colored rectangle. The
   Buildings-tab icon is
   `data/sprites/icons/building_<flatStem>.png`.

6. **Update the balance sheet.** Add the structure's row to the
   Structures table in `docs/BALANCE.md`, sourced from steps 2.
