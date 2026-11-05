# Can a player reach a match and issue one move order from what the screens say, and does every screen name the next action?

## Test contract

| Field | Value |
|---|---|
| Date | 2026-11-05 23:06 local |
| Build | `prj/bin/Release/conflict-converge.exe` (Release, 60 FPS shown in-match) |
| Commit | `2066fcb` (PR #77 merged), working tree clean: yes (`plans/` untracked) |
| Client | 1200x675; window origin (352,147); client offset (8,31) |
| Run type | smoke |
| Question | Can a player reach a match and issue one move order from what the screens say, and does every screen name the next action? |
| Budget | one match, ~20 actions, ~20 min wall-clock; consumed 7 actions, ~7 min |
| Gates | `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, allowlist `conflict-converge.exe` |

## Runs

One row per run. A claim that drives the decision needs two rows, or the note
says **observed once**.

| Run | Started | Result | Capture prefix |
|---|---|---|---|
| 1 | 23:06 | completed: title -> setup -> match -> select -> order -> pause -> resume -> close | `C:\Users\MSI\AppData\Local\windows-agent-mcp\downloads\captures\conflict-converge-20261105-*` |

## Observations

Directly observed only: what was on screen, what was clicked, what appeared. No
interpretation. `Believed` is what was expected before acting. `Severity` is
impact, not frequency.

| # | When | Where | Action | Observed | Believed | Evidence | Severity |
|---|---|---|---|---|---|---|---|
| 1 | 23:06 | title | launch, capture | `CONFLICT CONVERGE`, five buttons, `Start Skirmish` at (600,224) | title screen per recipes | `...\conflict-converge-20261105-230638-545.png` | - (expected behaviour) |
| 2 | 23:06 | skirmish setup | click `Start Skirmish` (600,224) | `Skirmish setup`; map list, AI difficulty Easy/Medium/Hard, `Start match` greyed out at ~(504,394); **no visible text explains the disabled state or says what to do first**; `Twin Basins` row highlighted cyan under the resting cursor | a disabled button, possibly with a hint | `...\conflict-converge-20261105-230646-815.png`, `...-230646-944.png` | moderate |
| 3 | 23:07 | skirmish setup | click High Ridge row (600,120) | map highlighted, source line `by dev-team data/maps\high_ridge.map` appears under the list, `Start match` moves to ~(504,421) and looks enabled | row click selects and enables | `...-230702-962.png` | - (expected behaviour) |
| 4 | 23:07 | match | click `Start match` (504,421) | world with HUD: `Selection` (`No selection`), `Abilities` bar, `Production` (`Inf` tab: Rifle Infantry 25/0, Anti-Armor 30/5, Engineer 40/0, Medic 30/0; `Queue: 0`), `Stockpile` (Iron 539 Oil 361), minimap | into the world | `...-230716-079.png` | - (expected behaviour) |
| 5 | 23:07 | match, spawn | drag (70,80)->(140,130) | red selection rect around blue infantry; `Selection:` `Rifle Infantry HP 100/100 Team 0`; `Abilities` fills Hold/Guard/Patrol/AttackMove/Halt (Guard marked active); combat visible elsewhere: a `-5` damage pop, enemy red contact | squad selects, panels update | `...-230742-914.png` | - (expected behaviour) |
| 6 | 23:08 | combat zone | right-click (600,300) | delta 0.4408; squad **left the spawn box**; in the next frame the selection markers sit on infantry at ~(440,45); a green dot near (600,290) fits the ordered point but green dots also appear in frames with no order; the enemy wave reached the base approach and the selected unit later reads `HP 10/100` | squad walks toward (600,300), move marker appears | `...-230755-557.png`, `...-230852-769.png` — **observed once** | moderate — order landing unverifiable during combat |
| 7 | 23:09 | match | key `p` | `Paused` modal: Resume, Camera speed 400, Minimap, Shortcut hints, volume sliders, Mute, Right-drag pan, Color-blind mode, Remap hotkeys..., Quit to menu, Quit to desktop | pause modal per recipes | `...-230909-367.png` | - |
| 8 | 23:09 | paused world | capture pair 9 s apart | Stockpile `Iron: 873 Oil: 473` identical in both frames (sim frozen); but 2,386 px differ (0.29%, bbox (198,5)-(394,487)) — soldier sprites animate while paused; **observed once**, contradicts recipes R3 "0.0 delta" | 0.0 delta while paused | `...-230923-046.png` vs `...-230932-059.png` | minor — noticeable, no outcome change |
| 9 | 23:10 | match | key `p` again | modal gone, HUD back, Stockpile `Oil: 473 -> 474` (sim running) | resume restores | `...-231049-813.png` | - |

## Positive observations

What worked and should be protected while fixing the problems.

- The path title -> setup -> match is three clicks, and every click landed
  where the recipes promised; all deltas matched the recorded values.
- Every in-match panel is labelled and self-explanatory (Selection, Abilities,
  Production, Stockpile); selection names the unit and HP type immediately.
- Selecting a map gives immediate feedback (source line) and visibly enables
  the next action.
- Pause/resume with `p` is a reliable toggle and the sim state is provably
  frozen (Stockpile counter motionless).

## Interpretation

- From obs 2 + 3: on Skirmish setup, the disabled `Start match` gives no
  on-screen reason or hint about what to do first. The screen only names the
  next action **after** a selection exists (obs 3, source line). Before it, a
  first-time player must guess that the map list is the blocker. This drives
  the decision.
- From obs 6: a move order during live combat did not visibly place the squad
  at the ordered point within the observation window. Combat engagement
  (Guard auto-defense) is a candidate cause, but this is **observed once** and
  unattributed — it needs a dedicated note, not a fix claim here.
- From obs 8: the paused build is sim-frozen but not pixel-stable; idle sprite
  animation continues under the pause modal. Recipes R3's "0.0 delta" did not
  hold for this run.

## Fix candidates

One variable per row. These are proposals, not the decision.

| # | Observation | Change | Why | Risk |
|---|---|---|---|---|
| 1 | Obs 2 | In Skirmish setup, show a visible "select a map first" hint near `Start match` while it is disabled ([issue #79](https://github.com/NTHvt981/conflict-converge/issues/79)) | the screen names the next action before selection, not only after (obs 3) | hint placement must not crowd the button; small |
| 2 | Obs 8 | Key unit idle animation to the frozen sim clock instead of render time during pause | pixel-stable paused frames make freeze proofs and comparisons reliable | animation source refactor; low visual impact |
| 3 | Obs 6 | none yet — gather evidence first | the order-overridden-by-combat behaviour needs a controlled run before any change | acting on one ambiguous frame risks a wrong fix |

## Doc drift

Values that disagree between `docs/BALANCE.md`, `data/configs/*.json`, the
README, and the running build.

| Topic | Source A says | Source B says | Action |
|---|---|---|---|
| Medic unit | `data/configs/medic.json` defines Medic (60 HP, 0 attack); the in-match Production panel offers `Medic 30/0` (obs 4) | `docs/BALANCE.md` lists seven units without Medic; `README.md` says "7 unit types" | update the docs to include the Medic |
| Playtest skill tracking | `docs/playtest/README.md`: skill is "(local, gitignored)" | `git ls-files`: `.opencode/skills/playtest/SKILL.md` is tracked | correct the README wording |
| Paused freeze guarantee | `docs/playtest/recipes.md` R3: "paused frames 0.0 delta" | this run: 0.29% sprite change under the modal (obs 8) | re-run the freeze proof once before rewriting the recipe |

## Not tested

What the run did not cover, so the next note does not read as a complete
verdict.

- Fun, feel, difficulty tuning, or whether the AI pressure observed (obs 6) is
  fair — handover material for a human.
- `Veh`/`Bld` Production tabs; issuing abilities from the Abilities bar; the
  minimap camera jump (R4); load game, settings, map editor screens.
- Victory/Defeat dialogs, quicksave (`F5`) / quickload (`F9`).
- The move order against a clean (non-combat) map state — obs 6 says nothing
  about peacetime order reliability.

## Decision

The note ends here; the decision is the last word. Run 1 completed the smoke
path; every claim below is **observed once**.

```
DECISION: retune
BECAUSE:  Obs 2 — on Skirmish setup the disabled 'Start match' carries no on-screen
          reason or hint, so the screen does not name the next action until a map
          is selected (obs 3 shows it names actions once selection exists).
CHANGE:   Skirmish setup, disabled 'Start match' with no hint
          -> show a visible "select a map first" hint next to the disabled button.
CONFIRM:  next setup-screen smoke: before any map row is clicked, a visible
          'select a map' hint stands next to the disabled button; the source
          line flow (obs 3) is unchanged after selection.
REVERSE:  a clear screenshot showing the hint already present next to the
          disabled button on this build, or a note showing a first-time player
          reached selection without the hint and without hesitation.
HANDOFF:  open prj/bin/Release/conflict-converge.exe, click 'Start Skirmish', and
          look at the setup screen before touching a map row: no hint text is
          visible near the greyed button (captures
          conflict-converge-20261105-230646-815.png / -944.png); then select
          High Ridge and confirm the source line appears
          (conflict-converge-20261105-230702-962.png).
```
