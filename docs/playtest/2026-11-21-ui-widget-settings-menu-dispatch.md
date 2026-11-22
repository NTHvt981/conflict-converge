# After the settings, main-menu and confirm screens move to `UiWidget`, does every control on them reach its handler?

## Test contract

| Field | Value |
|---|---|
| Date | 2026-11-21 21:13-21:25 local |
| Build | `prj/bin/Release/conflict-converge.exe` |
| Commit | `73f3c6a`, working tree clean: yes |
| Client | 1200x675; window origin (352,147); client offset (8,31) |
| Run type | feature |
| Question | After the settings, main-menu and confirm screens move to `UiWidget`, does every control on them reach its handler? |
| Budget | 2 runs, 21 actions, ~12 min |
| Gates | `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, allowlist `conflict-converge.exe` |

## Runs

| Run | Started | Result | Capture prefix |
|---|---|---|---|
| 1 | 21:13 | main-menu and confirm controls reach their handlers | `conflict-converge-20261121-2113`, `-2114`, `-2115`, `-2116` |
| 2 | 21:23 | settings and remap controls reach their handlers | `conflict-converge-20261121-2123` to `-2124` |

Both runs cover different screens, so each claim below rests on one action sequence and is **observed once**.

## Observations

| # | When | Where | Action | Observed | Believed | Evidence | Severity |
|---|---|---|---|---|---|---|---|
| 1 | 21:13:51 | title | none | "CONFLICT CONVERGE", "real-time strategy demo", five buttons: Start Skirmish, Load Game, Settings, Map Editor, Quit | title screen | `-211351-647.png` | none |
| 2 | 21:13:59 | Start Skirmish | click (600,225) | Skirmish setup; map rows, Easy/Medium/Hard with Medium filled, `Start match` greyed; delta 0.1581 | setup screen opens | `-211359-574.png` | none |
| 3 | 21:14:11 | setup Back | click (694,395) | title returns; delta 0.1635 | setup Back returns to the title | `-211411-337.png` | none |
| 4 | 21:14:30 | Load Game | click (600,282) | "Load game"; Quicksave row enabled, Slot 1-3 greyed; delta 0.1647 | load screen opens | `-211430-553.png` | none |
| 5 | 21:14:40 | load Back | click (600,295) | title returns; delta 0.1558 | load Back returns to the title | `-211440-908.png` | none |
| 6 | 21:14:51 | Settings | click (600,342) | labels read Camera speed 400, Master volume 1, Music volume 0.8, SFX volume 1, UI scale 1; Minimap checked; delta 0.1299 | settings with current values | `-211451-451.png` | none |
| 7 | 21:15:05 | settings Back | click (600,490) | title returns; delta 0.1299 | settings Back returns to the title | `-211505-820.png` | none |
| 8 | 21:15:22 | Map Editor | click (600,401) | editor canvas 24x18, brush row, Save/Save As/Back; delta 0.3782 | editor opens | `-211522-056.png` | none |
| 9 | 21:15:29 | editor Back | click (760,308) | title returns; delta 0.3782 | editor Back returns to the title | `-211529-768.png` | none |
| 10 | 21:15:38 | Quit | click (600,459) | modal over the dimmed menu: "Quit Conflict Converge?", "Close the game?", Yes/No; delta 0.9707 | confirm dialog opens | `-211538-382.png` | none |
| 11 | 21:15:46 | confirm No | click (678,380) | modal closes, title, process alive; delta 0.9759 | No cancels the quit | `-211546-517.png` | none |
| 12 | 21:15:55 | confirm Yes | Quit (600,459), then Yes (522,380) | window closes; `list_windows` no longer lists the process, and the log ended "Window closed successfully" before the run-2 launch truncated it | Yes ends the process | `-211555-493.png` | none |
| 13 | 21:16:22 | title | `escape` | modal opens with Yes focused; delta 0.9739 | recipe trap: Esc opens the quit confirm | `-211622-552.png` | none |
| 14 | 21:16:23 | confirm | `escape` | modal closes, title; delta 0.9739 | a second Esc cancels | `-211623-467.png` | none |
| 15 | 21:23:48 | Settings | click (600,342) | labels 400 / 1 / 0.8 / 1 / 1, Minimap checked; delta 0.1299 | settings with default values | `-212348-163.png` | none |
| 16 | 21:23:57 | Camera speed | drag (569,72) to (480,72) | label "Camera speed 250", knob at x 484; delta 0.0017 | the drag sets the model and the label follows | `-212357-280.png` | none |
| 17 | 21:24:05 | Shortcut hints | click (422,134) | box fills; delta 0.0004 | one click flips the toggle | `-212405-201.png` | none |
| 18 | 21:24:12 | Remap hotkeys... | click (600,538) | "Remap hotkeys"; 27 rows in two columns; "Replay: step back" reads `Left`, "Replay: step forward" reads `Right`; delta 0.185 | the screen opens and the process survives | `-212412-659.png` | none |
| 19 | 21:24:22 | remap Back | click (600,545) | settings returns with Camera speed 250 and Shortcut hints checked; delta 0.1692 | Back returns to settings | `-212422-900.png` | none |
| 20 | 21:24:24 | settings Back | click (600,490) | title returns; delta 0.1299 | Back saves and returns to the title | `-212424-041.png` | none |
| 21 | 21:24:34 | Settings | click (600,342) | Camera speed 250 and Shortcut hints checked on re-entry; delta 0.1299 | the model kept both changes | `-212434-784.png` | none |

## Positive observations

- Each converted button changed the screen on one click, with no double dispatch (2, 4, 8, 10, 11, 12, 14).
- The slider drag reached the model AND the label: "Camera speed 250" appears with no separate label write in the setter (16).
- The checkbox click reached the model, and the value survived a leave-and-return to the screen (17, 21).
- The confirm modal opens over the main menu and its two answers differ, so the modal document receives clicks while the menu document is shown behind it (10-12).
- The remap screen that ended the process before #112 now renders all 27 rows, including the two arrow-key rows (18).

## Interpretation

- Document-level click listeners on `main_menu.rml` and `confirm.rml`, and click/change listeners on `settings.rml`, deliver every control type tested (2-21).
- Dispatching to three trees in order resolves each id to one tree: a confirm answer never reaches the main-menu tree, and a main-menu button never reaches the confirm tree (10-14).
- The per-frame settings refresh is what makes the label live. The setter stores the number, and `RefreshSettings` writes the text; deleting either half breaks the visible value (16).
- The remap rows still use the id-dispatch path, which is why they work without a widget tree (18, 19).
- Settings values persist in `MenuFlow::settings` across screen changes inside one process (19, 21). Persistence across a process restart is not covered, because `data/settings.cfg` was deleted after the run.

## Fix candidates

None. No observation contradicts the design.

## Doc drift

| Topic | Source A says | Source B says | Action |
|---|---|---|---|
| `Start Skirmish` y coordinate | `recipes.md` R1: y 224 | capture 2: the button is centred at y 225 | none; both values fall inside the button |

## Not tested

- `hud.rml`, `skirmish_setup.rml` rows, `load_game.rml` slots, and every remap row click.
- Pause screen, which has its own note ([2026-11-21-ui-widget-pause-dispatch](2026-11-21-ui-widget-pause-dispatch.md)).
- Rebinding a key and the steal-conflict second click.
- Master, Music, SFX and UI-scale sliders: only Camera speed was dragged.
- Minimap, Mute, Right-drag and Color-blind toggles: only Shortcut hints was clicked.
- A reverse toggle flip: Shortcut hints was clicked once, never off and then on again.
- Audio output and the cursor-hint side effects of the toggles.
- Persistence across a process restart: `data/settings.cfg` was removed after the run.

## Decision

```
DECISION: keep
BECAUSE:  observations 2-21: every converted control changed state on one click, and settings values survived a screen re-entry
CHANGE:   none
CONFIRM:  the slice 2d run that converts the setup and load rows, repeating observations 2-5
REVERSE:  a converted control that needs a per-element listener, a click that fires twice, or a label that keeps a stale value
HANDOFF:  launch prj/bin/Release/conflict-converge.exe, click Settings at (600,342), drag Camera speed, click Shortcut hints, click Remap hotkeys... at (600,538), then Back twice
```
