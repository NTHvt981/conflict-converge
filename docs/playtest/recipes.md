# Playtest Recipes

Click paths verified against the running build, plus the traps each one
exposed. Coordinates are **client pixels** inside the 1200x675 client area;
`send_input` maps them with `ClientToScreen` at injection time, so never add the
window origin or the client offset yourself.

Verified 2026-11-05 on commit `e0a29cc`, Release build, client 1200x675, window
at (352,147), client offset (8,31). Re-verify before trusting a coordinate if
the client size, the map list, or the menu layout changed.

Deltas below are what that run produced, not thresholds. They move with the
scene; open the frames.

Each block lists actions in order. Send them one per `send_input` call and open the
after frame between them; a block is a sequence, not a batch.

## R0 — Check the surface

```text
list_windows()
  -> find the entry whose process is conflict-converge.exe
capture_window(window="conflict-converge.exe")
  -> read the PNG: is it the title screen, a match, a dialog?
```

The title is always `raylib basic window`. Match on process. A minimized window
needs `restore: true` before capture.

## R1 — Title to match

Three actions, each in its own `send_input` call and verified from its own frame.
Expect the screen to change substantially at each step.

```jsonc
{"type": "click", "x": 600, "y": 224}    // Start Skirmish
{"type": "click", "x": 600, "y": 120}    // a map row (High Ridge in this run)
{"type": "click", "x": 504, "y": 419}    // Start match
```

Observed: `Start Skirmish` delta 0.1581; map row 0.0782; `Start match` 0.9591
into the world.

Traps:

- **Hover is not selection.** A map row highlights under the cursor while no map
  is selected, and `Start match` stays disabled. Selection adds a source line
  under the list (`by dev-team data/maps\high_ridge.map`).
- **That source line moves `Start match`.** With no map selected the button sits
  at y 392; after selecting one it is at y 419. Click what the fresh frame
  shows, not a remembered row.
- **The map list can grow.** Rows start at y 87 and step 33 px; a save in
  `data/settings.cfg` can change which map is preselected.
- **The intro camera pans for a second or two** after the match appears. Wait
  for it to settle before reading battlefield coordinates.

## R2 — Select and order

Locate your units in the fresh frame; the drag box is map-dependent. One call per
action.

```jsonc
{"type": "drag", "from": [70, 80], "to": [140, 130]}
{"type": "click", "x": 600, "y": 300, "button": "right"}
```

Observed: drag delta 0.0281, green box drawn, Selection panel changes from
`No selection` to `Rifle Infantry HP 100/100 Team 0`, Abilities fills with
Hold/Guard/Patrol/AttackMove/Halt. Right-click delta 0.0329, a move marker
appears, the squad walks.

Traps:

- Battlefield coordinates depend on map, spawn, zoom, and camera. Read them
  from the frame you just captured; a box from an earlier note misses.
- A move order moves the units, not the camera. Do not compare the whole frame as
  if the map shifted.
- The selection panel is the cheap confirmation: it names the unit and HP.

## R3 — Freeze, judge, resume

`p` opens the pause menu and stops the simulation. Press it once, capture twice at
least a second apart, compare, then press it once more to resume.

```jsonc
{"type": "key", "key": "p"}              // pause; delta 0.8445, modal appears
{"type": "key", "key": "p"}              // resume; delta 0.8446
```

Observed: paused frames 0.0 delta across roughly 4 s. This is the only reliable
way to compare two states while the match runs.

Traps:

- The pause menu covers the map centre and contains **Quit to menu** and
  **Quit to desktop**. Never click there unless leaving is the intent; resume
  with `p`.
- `p` is a toggle. One press too many resumes the sim and the "frozen" frames
  start moving again.
- Pausing is not a save: quicksave is `F5`, load is `F9`.

## R4 — Camera jump by minimap

```jsonc
{"type": "click", "x": 1042, "y": 100}
```

Observed: delta 0.5495, the view jumps to the clicked minimap location. Use it
to bring a distant fight into the frame without panning.

Trap: it invalidates every battlefield coordinate you measured before the jump.

## R5 — Reset between runs

Close with process control, not with the UI:

```powershell
Stop-Process -Name conflict-converge -ErrorAction SilentlyContinue
```

Then launch `prj/bin/Release/conflict-converge.exe` from the repo root and
capture the title screen again.

Traps:

- `escape` on the title screen opens a **quit confirmation**, not a cancel.
- **Quit to desktop** in the pause menu also ends the process, but it is a UI click
  that can miss; process control is deterministic.
- Settings persist in `data/settings.cfg`; the next launch can differ in the selected
  map and difficulty.
