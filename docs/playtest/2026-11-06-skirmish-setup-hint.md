# Does the disabled Skirmish `Start match` show a cursor hint before selection and none after?

## Test contract

| Field | Value |
|---|---|
| Date | 2026-11-06 00:06-00:09 local |
| Build | `prj/bin/Release/conflict-converge.exe` |
| Commit | `33ac981`, working tree clean (code; only untracked `plans/`) |
| Client | 1200x675; window origin (352,147); client offset (8,31) |
| Run type | feature (issue #79 `CONFIRM`) |
| Question | Does hovering the disabled `Start match` show "select a map first" next to the cursor, and does the hint never appear once a map is selected? |
| Budget | 1 screen (setup), 4 actions + 3 captures, ~2 minutes |
| Gates | `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, allowlist `conflict-converge.exe` |

## Runs

One row per run. A claim that drives the decision needs two rows, or the note
says **observed once**.

| Run | Started | Result | Capture prefix |
|---|---|---|---|
| 1 | 00:06:59 | both halves of the `CONFIRM` passed | `conflict-converge-20261106-0007..0008` |

## Observations

Directly observed only: what was on screen, what was clicked, what appeared. No
interpretation. `Believed` is what you expected before acting, written down so a
surprise survives. `Severity` is impact, not frequency.

| # | When | Where | Action | Observed | Believed | Evidence | Severity |
|---|---|---|---|---|---|---|---|
| 1 | 00:07:07 | title | click `Start Skirmish` (600,224) | `Skirmish setup`: map list, `Medium` preselected, `Start match` greyed at ~(497,396); no text near the button explains the grey; cursor rests over the map list, `Twin Basins` row highlighted cyan with no source line under the list | setup screen opens with the button disabled until selection | `...-after-20261106-000707-109.png` | - (expected) |
| 2 | 00:08:00 | setup | click the disabled `Start match` (497,396) to rest the cursor (no `move` action exists; a click is the only cursor transport) | a white bordered box `select a map first` appears with its top-left just below-right of the cursor (~511,418); nothing else changes: button still greyed, no source line, no state change from the click | hover on the disabled button shows the hint; the disabled click stays a no-op | `...-after-20261106-000800-978.png`, `...-20261106-000803-691.png` | - (expected) |
| 3 | 00:08:36 | setup | click `High Ridge` row (600,120) | row turns selected-cyan; source line `by dev-team data/maps\high_ridge.map` appears under the list; `Start match` sits at ~(497,420) in the enabled style; no hint anywhere (cursor is on the row) | selection enables the button and moves it down ~27px | `...-after-20261106-000836-763.png` | - (expected) |
| 4 | 00:08:47 | setup | drag from empty (497,600) to (497,420) to rest the cursor on the now-enabled `Start match` (drag press and release land on different elements, so no `click` fires on the button) | the button shows the light-blue hover style; the run stays on the setup screen (map still selected); no hint on or near the cursor | the hint never draws once the button is enabled, even under hover | `...-after-20261106-000847-211.png`, `...-20261106-000850-384.png` | - (expected) |

Observations 2 and 4 are each **observed once**.

## Positive observations

- The hint appears and disappears without moving anything else: list rows,
  difficulty row, and button positions are identical across obs 1-4 frames with
  the hint present and absent.
- The disabled click in obs 2 did nothing — the hint does not make the button
  behave as clickable.

## Interpretation

- Hovering the disabled `Start match` shows `select a map first` next to the
  cursor (obs 2, observed once). The box anchors offset below-right of the rest
  point, matching the as-built description in `docs/features/skirmish-setup-hint.md`.
- Once a map is selected, hovering the enabled button draws no hint (obs 3 + 4;
  the hover state itself is obs 4, observed once).
- The issue #79 `CONFIRM` — "a setup-screen smoke where the hint shows under
  hover before selection and is gone after selection" — passes.

## Fix candidates

One variable per row. These are proposals, not the decision.

| # | Observation | Change | Why | Risk |
|---|---|---|---|---|
| (none) | | | | |

## Doc drift

Values that disagree between `docs/BALANCE.md`, `data/configs/*.json`, the
README, and the running build.

| Topic | Source A says | Source B says | Action |
|---|---|---|---|
| (none) | | | |

## Not tested

What the run did not cover, so the next note does not read as a complete
verdict.

- Hover at screen edges and corners (the position clamp), window resize, and
  non-default uiScale.
- The hint while the quit-confirm modal is open, and during the pick animation
  of the map list.
- Whether a fast pass over the button (no rest) shows the hint — the
  discoverability risk the issue accepts.

## Decision

The note ends here; the decision is the last word.

```
DECISION: keep
BECAUSE:  obs 2 shows the cursor hint exactly under the hovered disabled
          button and obs 4 shows no hint in the same hover once a map is
          selected — the issue #79 CONFIRM passes as written.
CHANGE:   n/a (keep)
CONFIRM:  n/a; any later doubt re-runs this feature run on a fresh build
REVERSE:  the hint drawing on the enabled button, drawing without hover, or
          shifting layout when it appears
HANDOFF:  10 minutes, human: build Release at 33ac981; start the game; click
          Start Skirmish; move the mouse onto the grey Start match and let it
          rest 1s (white box `select a map first` below-right of the cursor);
          click a map row; put the cursor back on the button (no hint); click
          Start match to confirm the match still starts.
```
