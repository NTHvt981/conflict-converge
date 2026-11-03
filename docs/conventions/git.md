# Git conventions

Rules for branches, commits, pull requests, issues, the wiki, and CI. Each idea is one
subsection with a Do example and a Don't example. When a change makes the repository disagree
with a rule, either the repository or this document changes in the same commit.

## Quick reference

| Area | One-line rule |
| --- | --- |
| [Branches](#branches) | `<type>/<kebab-slug>`; cut from `main`, target `main`, delete after merge |
| [Commits](#commits) | Imperative sentence case, no prefix, one self-contained step |
| [Messages](#wrap-the-body-at-72-columns) | Subject at most 72 columns, body wrapped at 72, `Closes #N` last |
| [Pull requests](#pull-requests) | Title mirrors the head commit; what, why, decisions, tests |
| [Templates](#add-no-pull-request-or-issue-templates) | No template files; copy the skeleton from this document |
| [Issues](#issues) | Title states the goal, one label from the set |
| [Wiki](#wiki) | Player-facing pages; design truth stays in `docs/` |
| [CI](#ci) | Push on `main`, pull requests on every branch, one run per commit |
| [Files](#files) | LF everywhere; build output and local notes stay untracked |

## Branches

### Name a branch `<type>/<kebab-slug>`

| Type | Use |
| --- | --- |
| `feature/` | new behaviour the player can see |
| `fix/` | a defect in shipped behaviour |
| `refactor/` | same behaviour, different structure |
| `docs/` | documentation, conventions, comments only |

**✅ Do**
```
feature/unit-yield-handshake
fix/halt-clears-transport-patrol
refactor/simulation-step-phases
docs/git-conventions
```

**❌ Don't**
```
stuff
patch-1
feature/issue-42
Feature/UnitYield
feat/unit-yield-handshake
```

The slug is lowercase kebab-case, two to five words, and names the change rather than the
issue number: the pull request carries `Closes #N`. Use `feature/`, never `feat/`.

### Cut from `main`, target `main`, delete after merge

**✅ Do**
```
git switch main
git pull --ff-only
git switch -c fix/halt-clears-transport-patrol
```

after the merge:

```
git branch -d fix/halt-clears-transport-patrol
git push origin --delete fix/halt-clears-transport-patrol
```

**❌ Don't**
```
git commit -m "quick fix"          # on main
git switch -c refactor/x feature/y # stacked on an unmerged branch
# merged three weeks ago, still on origin
```

The merge commit keeps every branch commit, so a deleted branch loses nothing. The branch list
stays a list of work in progress, and a stale remote branch makes it look like work is open.

## Commits

### Write an imperative subject with no prefix

**✅ Do**
```
Clear transport and patrol orders on halt
Prune AI harvester tracking by type and team
```

**❌ Don't**
```
fix: cleared some stuff
Fixed halt
[HALT] transport/patrol
```

Sentence case, present tense, no trailing period, at most 72 columns (50–60 keeps the log
readable). The subject says what the commit does to the code, not which area it touches.

### Keep one self-contained step per commit

**✅ Do**
```
a37bf40 Give the pool components one get-or-add
        "No call sites change yet: the Find* forwarders are still here for the sweep commit."
d220d23 Read pool components through Registry::Get
```

**❌ Don't**
```
one commit: new template + 164 call-site rewrites + an unrelated HUD fix
```

A reader bisecting the history needs every commit to build and pass on its own. A mechanical
sweep that changes no behaviour is its own commit, and the commit before it says so.

### Wrap the body at 72 columns

**✅ Do**
```
Tracked harvester ids survived id recycling: a non-Engineer that inherited a
dead harvester's id kept counting toward the quota, so the commander stopped
replacing its lost economy.

Fixes #16.
```

**❌ Don't**
```
one 300-column paragraph, then a bullet list of file names, then "tests: 4942 pass, 0 fail"
```

One blank line after the subject, prose paragraphs (bullets are for pull requests), and the
issue trailer on the last line: `Fixes #16.` for a defect, `Closes #18` for a task, `Part of #41.`
when the commit is one step of a larger issue. Rationale belongs in the body; verification
counts do not.

## Pull requests

### Title mirrors the head commit subject

**✅ Do**
```
PR:     Clear transport and patrol orders on halt
commit: Clear transport and patrol orders on halt
```

**❌ Don't**
```
PR:     Fix #16
```

### Body: what changes, why, design decisions, tests

**✅ Do**
```
## Why
HaltSelection left hasLoadOrder and hasPatrol set, so the order driver re-issued movement.

## What changed
- HaltSelection clears load, unload, and patrol state.

## Design decision
Stance and the toggle flags stay untouched, matching the existing halt contract.
```

**❌ Don't**
```
Checks: 4942, failures: 0
Debug/Release green
- [x] build passed
- [x] tests passed
```

Sections follow the change: a defect leads with why it happened, a refactor with what moved.
Name behaviours that are deliberately preserved. Verification stays in the conversation report,
never in the description.

### Start from this skeleton

```
## Why
<what the code did, and the symptom a player or reader saw>

## What changed
- <one line per behaviour change>

## Design decisions
- **<decision>.** <why it went this way, and what was deliberately preserved>

## Tests
<the case that would have caught it>

Closes #N
```

Delete the sections the change does not have: a docs-only pull request keeps two lines of
prose and `Closes #N`, and empty headings are worse than a short body. Swap `Why` for `Problem`
when the defect needs a reproduction, and lead a refactor with `What changed`.

### Add no pull-request or issue templates

**✅ Do**
```
.github/workflows/ci.yml   # the only file under .github
```

**❌ Don't**
```
.github/pull_request_template.md
.github/ISSUE_TEMPLATE/bug_report.md
```

A template fixes one heading set for every change, so a one-line fix grows empty sections and
the headings drift from this document. The body shape lives here, in one place, and each author
keeps the sections the change actually needs.

### Run the full gate before opening

**✅ Do**
```
premake5 vs2022
msbuild prj/ConflictConverge.sln /p:Configuration=Debug /p:Platform=x64
msbuild prj/ConflictConverge.sln /p:Configuration=Release /p:Platform=x64
prj/bin/Debug/conflict-converge-test.exe
prj/bin/Release/conflict-converge-test.exe
prj/bin/Debug/conflict-converge-e2e.exe
```

**❌ Don't**
```
one Debug build, no test run, "CI will catch it"
```

## Issues

### State the goal in the title and pick one label

**✅ Do**
```
Make the class bodies readable          label: refactor
Clear transport and patrol orders on halt   label: bug
```

**❌ Don't**
```
it crashes sometimes!!!
bug enhancement help wanted accessibility
```

| Label | Use |
| --- | --- |
| `bug` | shipped behaviour is wrong |
| `enhancement` | new behaviour |
| `refactor` | same behaviour, different structure |
| `documentation` | docs, comments, conventions |
| `accessibility` | barrier affecting people with disabilities |

A pull request closes its issue with `Closes #N` on the last line of the body, so the issue
closes on merge and the two stay linked.

### Start from this skeleton

```
## What happens
<the symptom, with the map, difficulty, or save slot that shows it>

## Steps to reproduce
1. <step>

## Expected
<what should happen instead>

## Where
<file, subsystem, or screen, when known>
```

A feature issue drops `Steps to reproduce` and says what the player should be able to do.
`Where` is optional; leave it out rather than guessing.

## Wiki

The wiki is a separate repository, `conflict-converge.wiki.git`, checked out beside the code.

### Keep the wiki player-facing and the repository authoritative

**✅ Do**
```
Home.md          index: Getting Started, Controls, Units
Controls.md      every default binding
Units.md         costs, stats, roles, damage matrix
```

**❌ Don't**
```
copy docs/features/orders-and-queueing.md into a wiki page
```

The wiki explains the game to a player: how to build it, what the keys do, what a unit costs.
Design and engineering documentation stays in `docs/`, next to the code it describes, and the
wiki links to it with a GitHub blob URL.

### Name pages Title-Case-Hyphen and link by bare page name

**✅ Do**
```
Getting-Started.md
[Controls](Controls)
```

**❌ Don't**
```
controls_page.md
[Controls](Controls.md)
```

### Commit in the wiki the same way

**✅ Do**
```
Add Home index, Controls and Units pages; note storage cap
```

**❌ Don't**
```
update
```

## CI

### One run per commit: push on `main`, pull requests on every branch

**✅ Do**
```yaml
on:
  push:
    branches: [main]
  pull_request:

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

**❌ Don't**
```yaml
on:
  push:
  pull_request:
```

A bare `push:` builds every branch head, and `pull_request` builds the same commit again, so
each push to a pull-request branch costs two full Debug+Release jobs. The concurrency group
cancels a superseded run on the same ref instead of letting both finish.

### CI owns the Debug and Release build and test run

**✅ Do**
```
msbuild prj/ConflictConverge.sln /p:Configuration=Debug   /p:Platform=x64
msbuild prj/ConflictConverge.sln /p:Configuration=Release /p:Platform=x64
prj/bin/<Config>/conflict-converge-test.exe
```

**❌ Don't**
```
add a job for conflict-converge-e2e or the ASan configuration
```

The e2e binary needs a GPU window and ASan costs 15+ minutes on a cold cache; both stay on the
local gate. CI answers one question: do Debug and Release build and pass the headless tests.

### Change a workflow and its local equivalent together

**✅ Do**
```
.github/workflows/ci.yml  +  the matching command in AGENTS.md
```

**❌ Don't**
```
add a build step to ci.yml only, so a local run no longer matches CI
```

## Files

### Keep every file LF

**✅ Do**
```
.gitattributes: * text=auto eol=lf
# after a PowerShell write, re-normalize before staging
```

**❌ Don't**
```
commit CRLF, then a 579-line diff of whole-file noise
```

### Keep build output and local notes untracked

**✅ Do**
```
prj/  build/  deps/  .opencode/  AGENTS.md  *.ccpb  *.log
```

**❌ Don't**
```
git add -f prj/bin/Debug/conflict-converge.exe
git add AGENTS.md
```

## Checklist

- [ ] branch is `<type>/<kebab-slug>`, cut from `main`, `feature/` not `feat/`
- [ ] commit subject imperative, sentence case, no prefix
- [ ] body wrapped at 72 columns, issue trailer on the last line
- [ ] pull request title mirrors the head commit, body names the design decisions
- [ ] body and issue follow the skeletons above, with the unused sections dropped
- [ ] no pull-request or issue template file added
- [ ] no check counts or build output in the pull request body
- [ ] wiki change is in the wiki repository with bare page-name links
- [ ] CI still runs once per commit
- [ ] merged branch deleted locally and on `origin`
