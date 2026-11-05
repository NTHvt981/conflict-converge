---
name: playtest
description: >
  Runs an evidence-based playtest of Conflict Converge from the pixels: capture the
  window, inject one input action at a time, verify it in the before/after frames, and
  write a note that keeps observations separate from fixes and ends in one
  keep/retune/kill decision. Use when asked to playtest the game, smoke-test the loop,
  verify a feature in the running build, check balance observations against
  docs/BALANCE.md, or report what a screen communicates to a first-time player.
---

# Playtest

Drive the running build through `windows-agent-mcp` and leave a note a human can act on.
The deliverable is a decision, not a screenshot album.

You are a camera with hands. You can prove what happened on screen; you cannot judge
whether it was fun. Notes that respect that split get used; notes that guess at fun get
skimmed.

## What this can and cannot answer

| Question | Answerable? | How |
|---|---|---|
| Does clicking X reach Y? | yes | before/after frames |
| What does this screen say? | yes | read the frame, quote the text |
| Is this control discoverable? | yes, in a first-time read | attempt only what the frame invites |
| Did the unit, panel, or state change? | yes | compare frames; the delta is a hint |
| Is the frame frozen? | yes | two captures >= 1 s apart at 0.0 delta |
| How long from A to B? | bounded | capture cadence, about +/-1 s |
| Is this balanced? | observation only | counts and timings vs `docs/BALANCE.md` |
| Is it fun, does it feel right? | **no** | hand it to a human |
| Audio, art quality, netcode, frame time | **no** | out of reach |

You are not a beginner. You know every control in this repo. In a first-time read, say
what the frame told you before you use that knowledge, and record the moments where you
only knew what to do because you read the code. An agent that quietly recovers from
confusion is a worse tester than a confused human.

## Preconditions

- `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, and `WAMCP_GUI_PROCESSES` contains
  `conflict-converge.exe`. Without the input gate, `send_input` is not registered.
- Build: `prj/bin/Release/conflict-converge.exe`. Debug carries `CC_DEBUG` cheats and
  assert paths; say so if you ever test Debug.
- Record `git rev-parse --short HEAD` and the client size before the first action.
- Match windows on process, never title: raylib titles every window
  `raylib basic window`. The client is 1200x675 inside a 1216x714 frame at the default
  position.

## The loop

```
┌─ state ───────────────┐   capture_window -> open the PNG
│ what is on screen now │   never act on a remembered frame
└──────────┬────────────┘
           │  write down: what I see, what I expect to happen
┌──────────▼────────────┐   send_input, ONE action per call
│ one action            │   settle_ms sized to the effect (menu 600-900, match ~4500)
└──────────┬────────────┘
           │
┌──────────▼────────────┐   open the after PNG, compare with before
│ verify                │   contradiction -> write it down before retrying
└──────────┬────────────┘
           │
┌──────────▼────────────┐   observation + belief + evidence path + severity
│ log one row           │
└──────────┬────────────┘
           └─────────> back to state (next capture)
```

Start the note before the first click, not after the run.

Every capture costs context: take one when a decision depends on what is on screen, not
on a timer. After an action, one after frame is enough.

Before the run:

- [ ] One question, one sentence, answerable by pixels.
- [ ] Run type chosen (below).
- [ ] Build, commit, client size, window origin recorded.
- [ ] Budget set: at most two matches, about 20 actions each.

## Hard rules

1. **One action per `send_input` call.** A batch hides which action failed.
2. **Capture immediately before choosing coordinates.** The intro camera, a map change,
   or a minimap jump invalidates any coordinate read from an older frame.
3. **Freeze with `p`, then judge.** Pause the sim, capture, compare, then `p` again. The
   pause menu covers the map and holds "Quit to menu" and "Quit to desktop": never click
   while paused unless that click is the intended action.
4. **Never send `escape` as a cancel.** On the title screen it opens the quit
   confirmation; in-match it only deselects.
5. **Open both PNGs.** A 3% delta is a normal order landing; 30% is often just animation.
6. **Write the contradiction down before the next action.** That row is the finding.
7. **Never re-run for a cleaner result.** The first run is the observation; a retry is a
   second row, not a replacement.
8. **No claim without a capture path and the on-screen text it rests on.**
9. **Do not edit the game during a run.** Fixes are candidates in the note.
10. **Bound the run.** The AI can destroy your force while you read frames.
11. **A control that does nothing is an observation, not an obstacle.** Do not click a
    disabled button repeatedly to force it.
12. **Release build, recorded commit.** Say which build every note describes.

## Evidence bar

| Claim | Minimum evidence |
|---|---|
| "the button works" | before frame plus after frame showing the next screen |
| "the unit moved" | the same unit in two different positions |
| "the panel updated" | readable text in the after frame |
| "the frame is frozen" | two captures >= 1 s apart, 0.0 delta, while paused |
| "input was ignored" | a second attempt from a fresh capture with the same result |
| "it takes about 20 s" | two timed events far enough apart to bracket it |

A claim seen once is labelled **observed once**. A claim that drives the decision is seen
twice, or the note says it was not.

## Run types

**Smoke** — the path still works: title, setup, match, one order, pause, quit. Report
each screen and whether the expected control was present and enabled. Do not test
balance here.

**Feature** — one feature, one path, every state visible: default, after one input,
after the reverse input, and the failure it is supposed to prevent. Quote the as-built
description in `docs/features/*.md` first, then check the same path; a difference is a
finding for the doc-drift table.

**Balance observation** — quote the expectation from `docs/BALANCE.md` and the value in
`data/configs/*.json` first, then measure in-match: counts, arrival times, time to kill.
A mismatch is a finding. "Too strong" is not.

**First-time read** — no source, fresh eyes: open the build and narrate what each screen
tells you about what to do next. Log every dead end. Count actions to the first match and
to the first unit order, and the number of dead ends; those counts are the finding. This
is the only mode where your ignorance is the instrument.

## Reading the frames

- Menu layouts are stable but not fixed: selecting a map adds a source line and moves
  `Start match` down about 27 px. Click what the fresh frame shows.
- Battlefield coordinates are never stable: they depend on map, spawn, zoom, and camera.
  Locate your units in the fresh capture, then drag.
- A highlighted row may be hover, not selection. Confirm with the text the frame adds.
- Combat, resource ticks, and the FPS counter all move pixels. Compare structures and
  text, not just totals.

## Writing the note

Write it to `docs/playtest/YYYY-MM-DD-<slug>.md`; a decision that lives only in chat is
not a playtest. Use `docs/playtest/_template.md`. Three rules carry the whole format:

1. **Observation and interpretation are separate sections,** and an interpretation
   cites the observation number it rests on.
2. **Fixes are candidates, one variable each,** never the decision itself.
3. **The note ends in exactly one decision:** keep, retune, or kill.

Severity is impact, not frequency: blocker (cannot proceed), major (repeated failure or
a materially wrong decision), moderate (friction with a workaround), minor (noticeable,
no outcome change).

## The decision gate

```
DECISION: keep | retune | kill
BECAUSE:  one cited observation, or the count across runs
CHANGE:   one variable, old -> new                     (retune only)
CONFIRM:  the next note that would show the change worked (falsifiable)
REVERSE:  what would overturn this decision
HANDOFF:  what a human checks in ten minutes, with capture paths and exact steps
```

`keep` still needs evidence of what worked. `kill` names what is cut and what stays. Never
"keep, but also fix five things": the fixes are candidates, the decision is one.

## Stop conditions

Stop and write the note where it stands when:

- three consecutive actions have no visible effect from a fresh capture;
- a dialog appears that the run did not plan for (Victory, Defeat, quit confirmation);
- `FOCUS_REFUSED` or `FOCUS_LOST` arrives: do not retry in a loop;
- the budget is reached, or the AI is clearly winning while you read.

A partial run with an honest stop reason is worth more than a completed run with a
reconstructed story.

## Pitfalls

- **Chasing the delta.** It measures pixels, not intent. Open the images.
- **Trusting a remembered coordinate.** Re-capture; the camera moved.
- **`escape` as cancel.** It is not a cancel.
- **Clicking through the pause menu.** Two of those buttons end the session.
- **Reading animation as a response.** Pause before comparing.
- **Writing the fix first** and bending the observation to match it.
- **One run, one verdict.** Say "observed once" or run it twice.
- **Claiming fun.** Hand it over instead.

## References

- `docs/playtest/README.md` — protocol, naming, index, decision log.
- `docs/playtest/_template.md` — the note to fill in.
- `docs/playtest/recipes.md` — verified click paths and coordinates.
- `docs/playtest/evals.md` — what a good run of this skill looks like.
- `windows-agent-mcp` README — `capture_window` and `send_input` parameters, errors,
  and the three gates.
