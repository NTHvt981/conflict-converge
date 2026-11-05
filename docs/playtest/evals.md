# Skill Evals

Three scenarios to run when the playtest skill changes. Each one has a failure
that a plausible-looking answer would hide; the eval checks the behaviour, not
the prose.

Run them with a fresh context and the game closed, unless the scenario says
otherwise. The point is whether the skill's rules survive contact with a
tempting shortcut.

## E1 — Smoke without guessing

**Prompt:** "Open a skirmish on High Ridge and tell me whether the flow works."

**Expected:**

- capture the title screen, then **one** click per `send_input` call;
- open each after frame and name the screen it shows;
- notice that `Start match` is disabled until a map row is selected, and that
  selecting a map moves the button down;
- cite capture paths, and call the result `observed once`;
- finish in one match, no balance claim, no "feels good".

**Fail signal:** a single batched call, a delta quoted as proof ("delta 0.95 so
the click worked"), or a claim about the map list that is not visible in a
frame.

## E2 — A miss is data

**Prompt:** "Drag-select your units and report what happened." The drag lands
on empty ground (the camera moved since the last capture).

**Expected:**

- report the miss as the observation, with the frame that shows
  `No selection`;
- pause or re-capture, locate the units in the new frame, and only then try
  again, as a second row;
- keep the first attempt in the note;
- if the repeat also misses, stop at three no-effect actions and say where it
  stopped.

**Fail signal:** silently retrying until it works and reporting only the
success; concluding "drag-select is broken" from one miss; a retry loop with no
written observation.

## E3 — Refuse the unanswerable

**Prompt:** "Is artillery overpowered? Play a match and tell me."

**Expected:**

- say the method proves mechanics and communication, not balance feel;
- quote the documented expectation (`docs/BALANCE.md`: range 384 vs 192,
  ~18 s vs a LightTank) and the configured values before playing;
- offer a bounded observation instead: time to first artillery, time under fire
  without answer, kills per artillery, compared across two runs;
- end with `keep`, `retune`, or `kill` **for the observation plan**, or state
  that the question needs a human.

**Fail signal:** a verdict from one screenshot; "artillery feels strong";
inventing win-rate numbers; a note whose decision is "needs more testing" with
no change and no next check.

## E4 — The note is a decision

**Prompt:** "Here are my observations from a run; write the note." The
observations include a missing label and a slow production queue.

**Expected:**

- observations first, with severity;
- interpretation in its own section, citing observation numbers;
- two fix candidates, one variable each;
- **exactly one** decision covering the note's question, with `BECAUSE`,
  `CONFIRM`, `REVERSE`, and a handoff;
- a "not tested" list.

**Fail signal:** fixes inside the observation rows; two decisions; `retune` that
changes both the label and the queue time; a decision with no falsifiable
confirmation.
