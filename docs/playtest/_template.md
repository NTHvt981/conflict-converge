# <Question, one sentence>

## Test contract

| Field | Value |
|---|---|
| Date | YYYY-MM-DD HH:MM local |
| Build | `prj/bin/Release/conflict-converge.exe` |
| Commit | `<short sha>`, working tree clean: yes/no |
| Client | 1200x675; window origin (352,147); client offset (8,31) |
| Run type | smoke / feature / balance observation / first-time read |
| Question | <the one sentence this note answers> |
| Budget | <matches, actions, wall-clock> |
| Gates | `WAMCP_GUI=1`, `WAMCP_GUI_INPUT=1`, allowlist `conflict-converge.exe` |

## Runs

One row per run. A claim that drives the decision needs two rows, or the note
says **observed once**.

| Run | Started | Result | Capture prefix |
|---|---|---|---|
| 1 | | | |

## Observations

Directly observed only: what was on screen, what was clicked, what appeared. No
interpretation. `Believed` is what you expected before acting, written down so a
surprise survives. `Severity` is impact, not frequency.

| # | When | Where | Action | Observed | Believed | Evidence | Severity |
|---|---|---|---|---|---|---|---|
| 1 | | | | | | | |

Severity: **blocker** (cannot proceed), **major** (repeated failure or a
materially wrong decision), **moderate** (friction with a workaround),
**minor** (noticeable, no outcome change).

## Positive observations

What worked and should be protected while fixing the problems.

-

## Interpretation

What the observations mean, each citing an observation number. Confidence is
stated when it is not obvious.

-

## Fix candidates

One variable per row. These are proposals, not the decision.

| # | Observation | Change | Why | Risk |
|---|---|---|---|---|
| 1 | | | | |

## Doc drift

Values that disagree between `docs/BALANCE.md`, `data/configs/*.json`, the
README, and the running build.

| Topic | Source A says | Source B says | Action |
|---|---|---|---|
| | | | |

## Not tested

What the run did not cover, so the next note does not read as a complete
verdict.

-

## Decision

The note ends here; the decision is the last word.

```
DECISION: keep | retune | kill
BECAUSE:  <one cited observation, or the count across runs>
CHANGE:   <one variable, old -> new>            (retune only)
CONFIRM:  <the next note that would show it worked, falsifiable>
REVERSE:  <what would overturn this decision>
HANDOFF:  <what a human checks in ten minutes, with paths and exact steps>
```
