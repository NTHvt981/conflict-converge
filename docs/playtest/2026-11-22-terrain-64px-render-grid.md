# Does the 64px terrain grid render at one texel per world unit, with water animated from a single clock that pauses with the match?

## Test contract

| Field | Value |
|---|---|
| Date | 2026-11-22 21:03-21:58 local |
| Build | Run 1: `prj/bin/Release/conflict-converge.exe`. Run 3: `prj/bin/Debug/conflict-converge.exe` with the reveal cheat forced on |
| Commit | Run 1: `6671d0e` on `terrain/64px-render-grid`. Run 3: `dbb0a75`, a temporary merge of `main` into the branch, deleted after the run |
| Client | 1200x675; window origin (352,147); client offset (8,31) |
| Run type | feature |
| Question | Does the 64px terrain grid render at one texel per world unit, and does water animate from a single clock that pauses with the match? |
| Budget | 2 matches, about 20 actions |
| Gates | `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, allowlist `conflict-converge.exe` |

## Runs

| Run | Started | Result | Capture prefix |
|---|---|---|---|
| 1 | 21:03 | Match reached Defeat while frames were read; stopped | `conflict-converge-20261122-21*` |
| 2 | 21:09 | Aborted on operator request | `conflict-converge-20261122-21*` |
| 3 | 21:55 | Completed: reveal, motion, and pause freeze observed | `docs/playtest/evidence/2026-11-22-terrain-64px-render-grid/` |

## Observations

| # | When | Where | Action | Observed | Believed | Evidence | Severity |
|---|---|---|---|---|---|---|---|
| 1 | 21:03 | Title, setup | click Start Skirmish, click Crossroads, click Start match | setup lists 6 maps; selection adds `by dev-team data/maps\crossroads.map`; match starts, delta 0.9556 | the three-click path works | captures rotated out, see run 3 for the same path | none |
| 2 | 21:04 | Match | click minimap (1068,140) | camera jumps into fog; water is black until explored | water sits where the map file puts it, x 11-12 | rotated out | minor |
| 3 | 21:04 | Match | drag-select squad, right-click east, minimap jump | main view shows the water band only after the squad walks east | fog hides terrain art until explored | rotated out | none |
| 4 | 21:04 | Match | pixel probe of the water band | water fills x 274..529, exactly 256 px, in 5 ramp steps `#24323b..#41646e` | 2-tile river at 128 px per logic tile | probe output, see Interpretation | none |
| 5 | 21:04 | Match | pixel probe, grass and water | grass: 315 isolated horizontal + 401 isolated vertical single pixels in 46 000 px; water: 1 474 isolated vertical single pixels in 63 960 px | a 2x upscale cannot produce isolated single pixels | probe output | none |
| 6 | 21:04 | Match | pixel probe, 64 px quadrants of one grass logic tile | adjacent quadrants differ on 11.8-14.0 % of pixels | per-cell variant selection is active | probe output | none |
| 7 | 21:05 | Match | two captures 8 s apart with no input | frames byte-identical | sim frozen | rotated out | moderate |
| 8 | 21:07 | Match | capture with `restore: true` | a Defeat dialog "Your force was destroyed." was on screen all along | captures were stale while the window sat occluded | rotated out | moderate |
| 9 | 21:09 | Match | setup and move east twice | match running; window closed by the operator | the animation check would follow | rotated out | none |
| 10 | 21:55 | Debug match | launch with `REVEAL_MAP_TOGGLE` defaulted on | the whole map draws from the first frame, no veil, no exploration needed | the reveal path covers terrain, units, buildings, nodes, and the veil | `reveal-water-playing.png` | none |
| 11 | 21:55 | Debug match | click minimap (1068,140) | the river draws in the dark-teal ramp with 1 px crest lines; the grass beside it is the dark olive ramp | the branch tile set is the one loaded | `reveal-water-playing.png` | none |
| 12 | 21:56-21:57 | Debug match | two captures of the water band while the match ran | 2 441 of 38 400 water pixels changed (6.36 %) | the water animates | `water-frame-a.png`, `water-frame-b.png` | none |
| 13 | 21:57 | Debug match | press `p`, two captures 15 s apart | water crop: 0 of 36 100 pixels changed; grass: 0 of 136 800; whole frame 0.017 % | pause stops the water clock | `paused-a.png`, `paused-b.png` | none |
| 14 | 21:55 | Debug match | none | the same map on `main` draws bright green grass and bright blue water; the branch build draws dark olive grass and dark teal water | the two builds carry different tile sets, so the new art is provably in use | capture pair from the two builds | none |
| 15 | 21:53 | Tooling | `send_input` with `ctrl` + `grave` | rejected: the key table has no grave or OEM key | the cheat overlay opens with Ctrl+`` | tool error text | moderate |

Severity: **blocker** (cannot proceed), **major** (repeated failure or a materially wrong decision), **moderate** (friction with a workaround), **minor** (noticeable, no outcome change).

## Positive observations

- Setup to match takes three clicks and no dead ends (1).
- Grass and water read as one field: no visible seam at the 64 px cell borders (3, 11).
- The water palette stays inside the ground value window; it does not pull the eye off the units (11).
- The reveal cheat makes the whole map inspectable in one action, with the sim untouched (10).
- Pause freezes the water cleanly, with zero changed pixels across 15 s (13).

## Interpretation

- **1:1 rendering is confirmed.** Single-pixel features exist in both surfaces (5). A 2x upscale, at any sub-pixel offset, produces runs of two pixels and cannot leave an isolated pixel between two equal neighbours. Confidence: high.
- **The geometry is right.** The 2-tile river covers 256 px, so one logic tile is 128 px and holds 2x2 render cells (4).
- **Variants are per render cell.** Adjacent 64 px quadrants inside one logic tile carry different contents (6).
- **The new art is the one on screen.** The branch build and the `main` build differ in both ground and water palettes (14), so a fallback to the old 128 px art would have shown bright green and bright blue.
- **The water animates and freezes with the match.** 6.36 % of the water pixels change while running (12), and none change while paused (13). The pause gate holds because the clock advances only inside the sim step.
- **Cadence is not measured.** The capture pair in (12) sits about 26 s apart, so it proves motion, not the 0.15 s frame step.
- **Capture staleness is a harness trap, not a game fault.** `capture_window` returned a stale surface while the window sat occluded (7, 8).

## Fix candidates

| # | Observation | Change | Why | Risk |
|---|---|---|---|---|
| 1 | 7, 8 | Always capture with `restore: true` after about 5 s without input | a stale surface costs a whole run | brings the window forward, so it interrupts a human watching |
| 2 | 15 | Give the cheat overlay a second open key that the input harness can send, for example F1 | the overlay is unreachable from automated runs | one more key to document |
| 3 | 15 | Or add the grave key to the harness key table | no game change | harness-only work |

## Doc drift

| Topic | Source A says | Source B says | Action |
|---|---|---|---|
| cheat overlay key | `Cheats.h` / `CheatOverlay.cpp`: Ctrl+`` | `docs/playtest/recipes.md`: no cheat recipe | add a recipe once a sendable key exists |

## Not tested

- Frame cadence: the 0.15 s step, measured with a timed capture pair.
- Seam quality at the water/grass border at 2x zoom.
- Runtime cost of 4 draws per logic tile on the largest map.
- Debug assert paths under a full match; run 3 was short.

## Decision

```
DECISION: keep
BECAUSE:  observation 13 shows zero changed water pixels across 15 s paused, and 12 shows 6.36 % change while the match runs, on the new dark-teal art from 11 and 14
CHANGE:   none
CONFIRM:  a timed capture pair 0.15 s apart that steps the crest by exactly one frame, to measure cadence rather than mere motion
REVERSE:  any changed water pixel across two paused captures, or motion at a rate that does not match 0.15 s per frame
HANDOFF:  Debug build, REVEAL_MAP_TOGGLE on, Skirmish, Crossroads, start; click the minimap water column (about 1068,140); capture with restore, capture again about 0.15 s later, compare the water band; then press p and capture twice 1 s apart
```
