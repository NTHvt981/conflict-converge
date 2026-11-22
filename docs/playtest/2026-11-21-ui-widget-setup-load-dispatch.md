# After the setup and load screens move to `UiWidget`, does every map row, difficulty row, start control and load slot reach its handler?

## Test contract

| Field | Value |
|---|---|
| Date | 2026-11-21 22:21-22:44 local |
| Build | `prj/bin/Release/conflict-converge.exe` |
| Commit | `03012ab` (run 1); `1e1312f` content (run 2, tree dirty); `4691121` content (run 3, tree dirty) |
| Client | 1200x675; window origin (352,147); client offset (8,31) |
| Run type | feature |
| Question | After the setup and load screens move to `UiWidget`, does every map row, difficulty row, start control and load slot reach its handler? |
| Budget | 3 launches, 35 actions, ~23 min |
| Gates | `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, allowlist `conflict-converge.exe` |

## Runs

| Run | Started | Result | Capture prefix |
|---|---|---|---|
| 1 | 22:21 | index-built rows, difficulty and load slots do not dispatch; literal-id Back does | `-2221` to `-2223` |
| 2 | 22:31 | after the id-ownership fix: rows, difficulty, Start, Quicksave and the disabled slot behave | `-2231` to `-2235` |
| 3 | 22:43 | one document listener delivers a map row after the duplicate row listener is removed | `-2243` |

The defect claim rests on four observations in run 1 (2, 3, 4, 7). The repaired behavior rests on run 2, with the map-row selection repeated in run 3 (10, 21). Single-observation claims are **observed once**.

## Observations

| # | When | Where | Action | Observed | Believed | Evidence | Severity |
|---|---|---|---|---|---|---|---|
| 1 | 22:21:30 | title | click Start Skirmish (600,225) | "Skirmish setup"; six map rows Crossroads to Twin Falls 2v2; Medium filled; `Start match` grey; delta 0.1581 | setup opens | `-222130-282.png` | none |
| 2 | 22:21:55 | map list | click Crossroads (600,87) | row shows a hover fill only; no source line; `Start match` grey; delta 0.0306 | the row selects and the source line appears | `-222155-112.png` | blocker |
| 3 | 22:22:29 | map list | click empty space (1050,600) | hover fill clears; no source line; `Start match` grey; delta 0.0148 | the selection stays | `-222229-550.png` | blocker |
| 4 | 22:22:40 | difficulty | click Easy (474,342) | Medium stays filled; delta 0.0061 | Easy fills | `-222240-850.png` | blocker |
| 5 | 22:23:14 | setup Back | click Back (694,395) | title returns; delta 0.1659 | Back returns | `-222314-806.png` | none |
| 6 | 22:23:29 | Load Game | click (600,282) | "Load game"; Quicksave enabled; Slots 1-3 grey; delta 0.1647 | the load screen opens | `-222329-326.png` | none |
| 7 | 22:23:31 | Quicksave | click (600,66) | screen unchanged; Quicksave shows a hover fill; delta 0.0452 | the quicksave loads | `-222331-140.png` | blocker |
| 8 | 22:23:57 | load Back | click Back (600,295) | title returns; delta 0.1635 | Back returns | `-222357-013.png` | none |
| 9 | 22:31:52 | title | click Start Skirmish (600,225) | setup with the same six rows; delta 0.1581 | setup opens | `-223152-447.png` | none |
| 10 | 22:31:53 | map list | click Crossroads (600,87) | row fills; source line `by dev-team data/maps\crossroads.map`; `Start match` enables; delta 0.0782 | the row selects | `-223153-450.png` | none |
| 11 | 22:32:08 | difficulty | click (474,342) after a map selection | no change; the pointer sits on the "AI difficulty" label; the Easy box is 27 px lower because the source line moved the column; delta 0 | Easy stays at y 342 | `-223208-643.png` | minor |
| 12 | 22:32:11 | Start match | click (504,419) | match starts; world and HUD appear; delta 0.9558 | the match starts | `-223211-431.png` | none |
| 13 | 22:32:25 | pause | key P | pause modal opens; delta 0.9308 | pause opens | `-223225-037.png` | none |
| 14 | 22:32:37 | Quit to menu | click (600,519) | title returns; delta 0.9506 | the title returns | `-223237-467.png` | none |
| 15 | 22:32:49 | Load Game | click (600,282) | "Load game"; delta 0.1641 | the load screen opens | `-223249-287.png` | none |
| 16 | 22:32:51 | Quicksave | click (600,66) | screen unchanged; delta 0.0452 | the save on disk loads | `-223252-077.png` | major |
| 17 | 22:34:18-22:34:23 | fresh save | click Start Skirmish (600,225), High Ridge (600,120), Start match (504,419), key F5 | new match on High Ridge; `data/quicksave.ccpb` is 44,728 bytes | the save writes | `-223423-657.png` | none |
| 18 | 22:34:49-22:34:54 | Load Game, Quicksave | press P, click Quit to menu (600,519), Load Game (600,282), click Quicksave (600,66) | match appears on High Ridge; HUD reads Iron 543, Oil 360; delta 0.9558 | the fresh save loads | `-223454-859.png` | none |
| 19 | 22:35:21 | difficulty | click Easy (474,342) with no map selected | Easy fills; Medium and Hard grey; delta 0.028 | Easy fills | `-223521-028.png` | none |
| 20 | 22:35:35 | Slot 1 | click Slot 1 (600,130) | screen unchanged; Slot 1 stays grey; delta 0.0221 | the disabled slot stays inert | `-223535-414.png` | none |
| 21 | 22:43:40 | map list | click High Ridge (600,120) | row fills; source line `by dev-team data/maps\high_ridge.map`; `Start match` enables; delta 0.0782 | the row selects through the document listener | `-224340-436.png` | none |

## Positive observations

- Literal-id Back buttons reached their handler on the first build while index-built controls did not (2, 4, 7 against 5, 8). This narrowed the fault to ids built by `UiIds::WithIndex`.
- On the fixed build, one click selected a map row, wrote the source line and enabled `Start match` (10).
- `Start match` launched a match from the selected map (12).
- A fresh F5 save loaded through the Quicksave slot into a live High Ridge match with HUD values (17, 18).
- The greyed Slot 1 stayed inert (20).
- The map row selected through the single document listener after the duplicate per-item listener was removed (21).

## Interpretation

- The run-1 failures share one cause. Every id built by `UiIds::WithIndex` failed to match, while literal ids from `UiIds` worked (2, 4, 7 against 5, 8). A source read after the run showed `UiWidget` stored a `std::string_view`, and `WithIndex` returns a `std::string` temporary, so `Find` compared against freed storage.
- Owning the id repaired every index-built control in run 2: map row, difficulty row, Start, Quicksave and Slot 1 (10, 12, 19, 20).
- The first Easy click after a map selection missed because the source line moved the difficulty row 27 px down (11). The row sits at y 342 without the source line and at y 369 with it. The widget dispatch worked at the unshifted position (19).
- The stale Quicksave did not load because `LoadWorld` rejected the 22 September save with no log line (16). A fresh F5 save loaded on the same slot (17, 18). Slot dispatch was correct in both cases.
- Slot 1 re-checks `std::filesystem::exists` in its handler, so the greyed row stays inert even though the `disabled` class is visual only (20).
- Run 3 shows the document listener delivers the map row after the duplicate per-item registration is removed (21).

## Fix candidates

| # | Observation | Change | Why | Risk |
|---|---|---|---|---|
| 1 | 2, 4, 7 | `UiWidget` stores `std::string id_` | a computed id outlives the string it came from | one allocation per widget; applied between runs 1 and 2, committed as `1e1312f` |
| 2 | 16 | log a warning when `LoadWorld` rejects a save | a rejected save and a dead slot look identical | one log line per failed load; not applied |

## Doc drift

None. `recipes.md` R1 records that the source line moves `Start match` from y 392 to y 419.

## Not tested

- The other four map rows: Prototype Sandbox, Salt Flats, Twin Basins, Twin Falls 2v2.
- Slots 2 and 3, and a save other than Quicksave.
- The Watch last replay row.
- Hard difficulty; only Easy and the default Medium were selected.
- The 2v2 ally AI after a Twin Falls 2v2 load.
- The raygui fallback when a document fails to load.
- Frame pacing and visual quality.

## Decision

```
DECISION: keep
BECAUSE:  observations 10, 12, 18-21: on the fixed build, a map row, Start match, the Quicksave slot and the disabled Slot 1 each reached their handler on one click
CHANGE:   none
CONFIRM:  a later note that clicks the four remaining map rows and Slots 2-3
REVERSE:  an index-built control that fails to dispatch after `UiWidget` owns its id, or a filled slot that does not load
HANDOFF:  launch prj/bin/Release/conflict-converge.exe, click Start Skirmish at (600,225), click a map row, click Start match at (504,419), press F5, press P, click Quit to menu at (600,519), click Load Game at (600,282), click Quicksave at (600,66)
```
