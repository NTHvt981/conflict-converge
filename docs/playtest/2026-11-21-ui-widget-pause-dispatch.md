# Does the pause screen still dispatch every control through UiWidget?

## Test contract

| Field | Value |
|---|---|
| Date | 2026-11-21 17:26 local |
| Build | `prj/bin/Release/conflict-converge.exe` |
| Commit | `4aa767e`, working tree clean: no |
| Client | 1200x675; window origin (352,147); client offset (8,31) |
| Run type | feature |
| Question | After the pause screen moves to `UiWidget` with one document-level listener, do buttons, sliders and toggles still reach their handlers? |
| Budget | 1 match, 9 actions, ~2 min |
| Gates | `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, allowlist `conflict-converge.exe` |

## Runs

| Run | Started | Result | Capture prefix |
|---|---|---|---|
| 1 | 17:26 | pause controls reach their handlers | `conflict-converge-20261121-1726`, `-1727` |

## Observations

| # | When | Where | Action | Observed | Believed | Evidence | Severity |
|---|---|---|---|---|---|---|---|
| 1 | 17:26:58 | pause modal | `p` | "Paused", Resume, 4 sliders, 5 checkboxes, Remap/Quit buttons | modal over the map | `-172658-454.png` | none |
| 2 | 17:27:09 | Shortcut hints | click (486,234) | checkbox fills; hint overlay appears behind the modal ("WASD Camera", "Left Select", "Right Order", ...) | toggle flips and `showHints` reaches the HUD | `-172709-208.png` | none |
| 3 | 17:27:18 | Shortcut hints | click (486,234) | checkbox empties; overlay disappears | the reverse toggle also dispatches | `-172718-264.png` | none |
| 4 | 17:27:27 | Camera speed | click (540,186) | label `400` becomes `270`; knob moves to x 541 | slider sets the model and `RefreshPause` rewrites the label | `-172727-182.png`, delta 0.0022 | none |
| 5 | 17:27:51 | Resume | click (600,130) | modal closes; Stockpile iron 595 -> 612 | resume dispatches and the sim restarts | `-172751-521.png` | none |

## Positive observations

- A `change` event on a checkbox reaches the document-level listener and is classified as a toggle (2, 3).
- A `change` event on `input[type=range]` is classified as a slider value, and the value is already clamped into the 100..800 band (4).
- The value label updates from the refresh path, so deleting the immediate label write in the old change handler costs nothing visible (4).
- A `click` on a button reaches the document-level listener (5).
- No double dispatch: one click produces one state change (2, 3, 5).

## Interpretation

- Document-level `AddEventListener("click" | "change")` on `pauseDoc_` delivers both event types by bubbling, so the per-id `Listen` lists are not needed for this screen (2, 3, 5). Confidence: high, two event types and three controls.
- The `Translate` split on `dynamic_cast<Rml::ElementFormControl *>` plus `type="range"` survives real input (4).
- The widget layer adds no visible latency: each frame after the action already shows the new state at a 900 ms settle (2-5).

## Fix candidates

None. No observation contradicts the design.

## Doc drift

None found.

## Not tested

- "Remap hotkeys", "Quit to menu", "Quit to desktop". They share the `UiButton` construction with Resume, so this is code-level confidence only.
- Master/Music/SFX sliders and the other four checkboxes.
- Any screen outside `pause.rml`; `RmlUiMenus` and the HUD still use the id-dispatch path.
- `data/settings.cfg` is absent, so the camera speed change did not persist across the run.

## Decision

```
DECISION: keep
BECAUSE:  observations 2-5: click and change both reach their handlers through the widget tree
CHANGE:   none
CONFIRM:  the same run after slice 1 deletes the pause branches from OnClick
REVERSE:  a pause control that needs a per-element listener, or a click that fires twice
HANDOFF:  launch prj/bin/Release/conflict-converge.exe, Start Skirmish, pick a map, Start match, press P, click Shortcut hints and drag Camera speed
```
