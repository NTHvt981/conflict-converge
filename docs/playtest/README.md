# Playtests

One note per question, written from a run of the real build. A note keeps
observations separate from interpretation and fixes, and ends in exactly one
decision: keep, retune, or kill.

The agent skill that drives a run lives in `.opencode/skills/playtest/`
(local, gitignored). This directory is the versioned protocol and record, so a
note means the same thing on another machine and a year later.

## Files

| File | Holds |
|---|---|
| `README.md` | this protocol, the playtest index, the decision log |
| `_template.md` | the note to copy for a new run |
| `recipes.md` | click paths and coordinates verified against a build |
| `evals.md` | what a good run of the skill looks like; used when the skill changes |
| `YYYY-MM-DD-<slug>.md` | one playtest note |
| `evidence/<note-slug>/` | at most the decisive frames, when words lose the finding |

## Rules

1. **One question per note.** A note that answers three questions decides
   nothing. If a second question appears mid-run, finish this note and open
   another.
2. **Record before interpreting.** Observations come first and describe pixels:
   what was on screen, what was clicked, what appeared. Interpretation comes
   later and cites observation numbers.
3. **Fixes are candidates, one variable each.** A fix names the observation it
   addresses, the change, and the risk. It is never part of the decision line.
4. **One decision per note.** `keep`, `retune`, or `kill`. `retune` changes one
   variable. `kill` names what is cut and what stays.
5. **Evidence or it did not happen.** Every observation cites a capture path and
   the on-screen text it rests on. A claim seen once is labelled **observed
   once**.
6. **Append, do not rewrite.** A later note supersedes earlier ones by linking
   to them. The earlier decision stays as written.
7. **Write the note during the run.** A note reconstructed afterwards records
   what you already believe, not what happened.
8. **Reproducible in ten minutes.** A human can follow the note's steps,
   rebuild the same view, and see the same thing.

## Naming

`YYYY-MM-DD-<question-slug>.md`, for example
`2026-11-06-skirmish-start-button-disabled.md`.

## Scope of a note

A note proves mechanics and communication: a control reached the next screen, a
panel showed a value, a frame froze, a label was missing, a step took about
20 seconds. It does not prove fun, feel, audio quality, or overall balance.
Those go to a human with the note's evidence.

Balance observations compare three sources and report drift before opinion:

| Source | Read from |
|---|---|
| documented expectation | `docs/BALANCE.md` |
| configured value | `data/configs/*.json` |
| observed behaviour | the run's frames and timing |

A mismatch between the first two is a documentation finding. A mismatch with
the third is a behaviour finding. Neither is a verdict on fun.

## Evidence

Captures stay outside the repo, in the MCP download root
(`<download root>/captures/`, default
`C:\Users\<user>\AppData\Local\windows-agent-mcp\downloads\captures\`). A note
quotes screen text and cites the filename.

Copy a frame into `evidence/<note-slug>/` only when the finding is visual and
words would lose it (a missing label, a wrong icon, a layout collision). Two
frames per note is the budget.

## Index

| Note | Build | Question | Decision |
|---|---|---|---|
| _(none yet)_ | | | |

## Decision log

Append one row when a note is finished. The log is the history of what changed
because of a playtest.

| Date | Note | Decision | Change | Confirmed by |
|---|---|---|---|---|
| | | | | |
