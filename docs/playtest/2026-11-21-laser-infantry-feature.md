# Playtest — Laser Infantry feature pass

- Date: 2026-11-21
- Build: Release. Run 1 at `5777400`; runs 2 at `5777400` + uncommitted UI fix
  (fifth production row, see O1). Client 1200x675, window at (352,147).
- Run type: Feature (one feature: the laser infantry unit path)
- Question: can the player train, field, and command a Laser Infantry, and do the
  ENERGY beam, squad clustering, and restored 45-degree barrack render correctly?
- Budget: 2 matches, ~14 actions (run 1) and ~12 actions (run 2). Both matches
  ended in an unplanned Defeat dialog — stopped per protocol.

## Expectations (as-built)

- Production menu has 9 entries; Laser Infantry costs 35/10, builds in 2.7 s at
  the Bootcamp (`docs/BALANCE.md`, `data/configs/laser_infantry.json`).
- Laser renders as a rifle-style squad (5 figures at full health, 0.8 base scale).
- Beam: violet `#b48cff`, 0.12 s life, muzzle-anchored in all 8 facings,
  fog-gated (issue #108 section 2).

## Observations

Captures under `C:\Users\MSI\AppData\Local\windows-agent-mcp\downloads\captures\`.

- **O1 (blocker, fixed mid-run).** Run 1's Production panel listed 4 infantry
  buttons and no Laser Infantry (`conflict-converge-after-20261121-113029-638.png`).
  The RML template hard-codes 4 rows and the panel code takes(4) of the menu.
  Fixed between runs: fifth row in `hud.rml`, infantry loop take(4)->take(5),
  icon decorator + `icons/laser_infantry.png` (24x24, from the ingested blue
  idle flat). After the fix the button reads "Laser Infantry 35/10" with its
  icon in three frames of both runs
  (`...-113749-801.png`, `...-113759-499.png`, run-2 frames). The fresh build
  log loads `data/sprites/icons/laser_infantry.png` (texture ID 143).
- **O2 (inconclusive).** Run 1, click on the Laser Infantry button
  (`...-113759-499.png`): queue row read `QuBuf: 0` before and after; no new
  unit appeared in later frames, but the Bootcamp spawn area sat at the screen
  bottom edge, partially off-frame. Not a claim either way.
- **O3 (tint works; trigger unresolved).** Run 1's Defeat frame
  (`...-114019-133.png`) shows the Bootcamp with blue team tint (awning, flag,
  door) — the restored 45-degree barrack + mask pipeline tints correctly
  in-engine. The same building rendered gray at match start in both runs
  (`...-113749-801.png`, run-2 start). Gray -> blue transition unresolved;
  construction state is the candidate (the log warns
  `barrack_construction_base/mask.png` missing).
- **O4 (blocker for the train path).** Direct LMB clicks never selected a
  building: Depot footprint center (320,335) -> "No selection"
  (`...-114423-412.png`), Bootcamp (415,652) -> "No selection"
  (`...-114307-274.png`), drag box over the Depot -> "No selection"
  (`...-114553-305.png`), plus run-1 attempts on the Bootcamp. Five-plus
  attempts across both runs. `docs/playtest/recipes.md` documents unit
  selection only; no known building-select control was available to me.
- **O5 (major, out of feature scope).** Run 2 (Easy): the whole starting force
  (5 Rifle + LightTank + IFV + Engineer) was destroyed within about 2.5 minutes
  by roughly 8 riflemen standing at full HP inside my base
  (`...-114553-305.png`), with no combat observed near the units' start
  position. Run 1 (Medium) lost the same way at about 5 minutes. The starting
  force must have moved/auto-engaged off my observation window; unresolved.
  Minimap blue blobs were ambiguous (read as units or explored terrain).
- **O6.** Run 1: Defeat dialog "Return to menu" works (delta 0.9828, back to
  the title screen).

## Interpretation

- **I1 (from O1).** The production-panel row cap was a real integration gap the
  unit registration slice missed; the fix is required for acceptance and works.
  Commit it with this note.
- **I2 (from O3).** The barrack re-bake from the 45-degree source is visually
  correct in-engine. The gray-at-start look needs one follow-up question: is
  that an intentional construction state or a tint-path bug?
- **I3 (from O4).** The train path (select Bootcamp -> queue Laser) could not
  be exercised by pixels because building selection never landed. I do not know
  whether a working control exists and I missed it, or the select is broken —
  the branch does not touch selection, so this is likely pre-existing. The
  beam, save/load, and squad-clustering checks remain unrun behind it.
- **I4 (from O5).** The force-loss rate is pre-existing combat behavior, not
  the laser feature (no laser ever fielded). It contradicts the Easy-vs-Medium
  soak expectations and deserves its own run with the camera locked on the
  starting force.

## Fix candidates (one variable each)

- **C1.** Establish the intended building-selection control; if none exists,
  make LMB on a building footprint select it. (Not applied.)
- **C2.** Add `barrack_construction_base/mask.png` or make the construction
  state explicit, so the gray -> blue bootcamp transition is intentional and
  readable. (Not applied.)
- **C3.** Next-run recipe: Easy + Twin Basins, first action is the Laser button
  click, camera parked on the Bootcamp spawn area the whole time. (Test plan.)
- **C4 (separate note).** Watch the starting force from t=0 on Easy to explain
  O5 (stance/auto-engage vs incoming wave). (Test plan.)

## Decision

`DECISION: keep`
`BECAUSE: O1 + O3 — the button, icon, registration, and barrack tint are all`
`correct in-engine, and the suite/soak gates are green; the unverified part of`
`the path is blocked by the pre-existing building-selection question (O4), not`
`by laser code.`
`CONFIRM: the next note shows a queued Laser Infantry (QuBuf or spawn) and`
`captures the violet beam on a live hit.`
`REVERSE: if building selection is broken on main for all buildings, PR #109`
`keeps the feature but gains a known-issue note and O4 becomes its own bug.`
`HANDOFF: 10-minute human check — launch Release, start any skirmish, LMB each`
`building type, report which select; then click Laser Infantry and watch the`
`Bootcamp spawn area for the new squad.`
