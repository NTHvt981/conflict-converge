# Plan: Unit Automation (Micro Offload) QoL

## Implementation status: FULLY SHIPPED (updated 2026-09-17)

- Overkill protection (`3c69f29`), target priority (`6b51b98`),
  player auto-retreat (`492cdd5`), building auto-repair (`38f6f26`).
- All four proven soak-neutral (identical Medium-vs-Hard trajectories).
  Auto-repair shipped as the recommended global-toggle v1 (AI unwired —
  soak-tuned economy); ScoutTick fast-retry was tried and reverted as
  soak-load-bearing (see `ea2b6d7`).

Covers, from `missing_features.md` § "Unit automation (micro offload)":
- Overkill protection (don't pile shots onto a nearly-dead target)
- Target priority per unit (tanks prefer tanks over infantry)
- Auto-retreat at HP threshold for player units (AI already has `RetreatTick`)
- Auto-repair toggle on buildings (funds cap / % selector)

No UI/input dependencies — this is the safest plan to implement first (pure simulation-side
changes), and should be validated against the existing soak tests
(`tests/unit-tests/skirmish_tests.cpp`'s soak runs) since it changes combat outcomes.

---

## 1. Overkill protection

### Current behavior (why piling happens)
`AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr)`
(`src/game/public/units/Targeting.h:19`, impl `src/game/private/units/Targeting.cpp:12-48`) is a
**stateless, independent scan** — every seeking unit picks the enemy with highest `attackPower`
(ties broken by nearest distance), with zero awareness of what other units have already targeted.
Because the formula is deterministic, multiple attackers naturally converge on the same
highest-threat-then-nearest target. `ResolveAttack` (`Combat.h:32`, impl `Combat.cpp:19-36`)
directly decrements `defender.health` with no "already lethal" check.

### Design: frame-scoped "reserved lethal damage" map
Build this once per frame, before the per-unit `UpdateUnit` loop runs (inside
`RunUnitMovementFrame`, `Unit.h:226-227` / its `.cpp` definition — the function that already does
the occupancy pre-pass and per-unit driver loop):
```cpp
// Sum of attackPower from every unit currently mid-WindUp/Recover with a
// live `target`, keyed by target entity. Approximates "damage already
// committed to this target this cycle" without needing a full hit-landing
// simulation — good enough to stop new attackers from piling onto a target
// that's already got enough attackers assigned to kill it.
std::unordered_map<Entity, float> reservedDamage;
registry.Each<Unit>([&](Entity, const Unit &u) {
    if (u.health > 0.0f && u.target != kInvalidEntity &&
        (u.phase == AttackPhase::WindUp || u.phase == AttackPhase::Recover))
    {
        reservedDamage[u.target] += static_cast<float>(u.attackPower);
    }
});
```
Thread this into `AcquireTarget` as a new optional trailing parameter, following the exact pattern
already used for `fog`:
```cpp
Entity AcquireTarget(const Registry &registry, Entity seeker, const FogOfWar *fog = nullptr,
                     const std::unordered_map<Entity, float> *reserved = nullptr);
```
Inside the candidate loop (`Targeting.cpp:25-46`), skip (same as the existing dead/ally/fogged
skips) any candidate whose `health - (reserved ? (*reserved)[candidateId] : 0.0f) <= 0.0f`. Use
`reserved->find(id)` not `operator[]` to avoid mutating the map through a const pointer's
dereferenced non-const map — actually since `reserved` is `const std::unordered_map*`, use `.find`
and check `!= end()`.

### Wiring
Build `reservedDamage` once in `RunUnitMovementFrame` (or wherever the per-frame unit-update loop
lives), pass `&reservedDamage` through to every `AcquireTarget` call site inside `UpdateUnit`
(`Unit.cpp` — the attack-move contact scan and the main no-target branch, currently two call
sites). `AcquireBuildingTarget` (`Targeting.cpp:63-97`) is unaffected — buildings aren't unit
targets and this feature is scoped to unit-vs-unit piling per the feature description.

### Balance knob
`attackPower` is a proxy for "damage this attacker will eventually land," not the exact amount
(armor/damage-type effectiveness modifies actual damage via `Effectiveness`, `Combat.cpp:5-17`).
Using raw `attackPower` is a conservative approximation (may slightly over-reserve, causing a unit
to look elsewhere for a target it could actually still profitably finish) — acceptable for a QoL
pass; note in a comment that a more precise version would run `attackPower *
Effectiveness(attacker.damageType, candidate.armorType)` per reserving unit, which the plan doesn't
require but leaves room for.

### Tests
Extend `targeting_tests.cpp`: construct two attackers and one nearly-dead defender whose health is
less than one attacker's `attackPower`; have attacker A already mid-`WindUp` targeting the
defender; assert `AcquireTarget` for attacker B (with the reserved map populated) skips the
defender and picks a different, healthier target instead. Also assert existing tests (threat beats
proximity, ties go to nearest) still pass unchanged when `reserved` is `nullptr` — this is a
strictly additive, opt-in parameter.

---

## 2. Target priority per unit (tanks prefer tanks over infantry)

### Current behavior
`AcquireTarget`'s scoring (`Targeting.cpp:40-45`) is `unit.attackPower > bestThreat ||
(unit.attackPower == bestThreat && dist < bestDist)` — the *seeker's own type* plays no role at
all; every unit type scores candidates identically.

### Design: static weight matrix, mirroring `Effectiveness`
`Combat.cpp:5-17`'s `Effectiveness(DamageType, ArmorType)` is the direct precedent — a small
`static constexpr float` matrix keyed by two enums. Add the sibling in `Targeting.h/.cpp`:
```cpp
// Multiplier applied to a candidate's threat score based on the seeker's
// type — models "tanks prefer engaging tanks over infantry" etc. 1.0 =
// neutral. Mirrors Effectiveness's matrix shape (Combat.cpp:5-17).
float TargetPriorityWeight(UnitType seekerType, UnitType candidateType);
```
Populate the matrix conservatively for a first cut — e.g. armored seekers (`LightTank`,
`HeavyTank`, `IFV`, `Artillery`) get a >1.0 weight against other vehicle types and 1.0 (neutral)
against infantry (`Infantry`, `AntiArmorInfantry`, `Engineer`); `AntiArmorInfantry` gets >1.0
against vehicle types specifically (matches its name/role); everything else stays 1.0 across the
board (no behavior change) until playtesting suggests otherwise. Document the exact numbers chosen
in a comment — this is a balance decision, not a mechanical one, and should be easy to retune
without touching call sites.

### Wiring
One-line change at `Targeting.cpp:40`: replace the `unit.attackPower` comparand with
`unit.attackPower * TargetPriorityWeight(self->type, unit.type)`. `self` (the seeker's `Unit*`) is
already in scope at that point in the function (used earlier for team/position comparisons) — no
signature change needed for this half of the feature.

### Tests
Extend `targeting_tests.cpp:49-58`'s existing "threat beats proximity" test block with a new case:
two enemy candidates with equal `attackPower` but different `UnitType` (one armored, one
infantry), a `HeavyTank` seeker; assert it picks the armored candidate over infantry despite equal
raw `attackPower`, and that swapping the seeker to `Infantry` (neutral weights) falls back to the
existing nearest-wins tiebreak.

---

## 3. Auto-retreat at HP threshold for player units

### Exact AI logic to reuse (`AICommander::RetreatTick`, `src/game/private/units/AICommander.cpp`)
Search for `RetreatTick` in `AICommander.cpp` for the current exact line numbers (verified logic
as of this plan, ~lines 366-408):
- Threshold: hardcoded `0.3f` of `BaseStats(unit.type).health` (max HP **by type**, not a
  per-instance `maxHealth` field — units don't have one, only `Building` does).
- Exclusion: `UnitType::Engineer` never auto-retreats.
- Gate: only runs when `params_.retreats == true`, which is only true for `AIDifficulty::Hard`.
- Mechanism: `IssueAttackMoveOrder`/`IssueAttackMoveOrderFootprint` toward `homeTile_` — a
  **fighting withdrawal** (keeps firing while walking home), deliberately not a plain move order,
  per a comment there citing a measured soak regression from using plain move instead.
- Re-path guard: skips re-issuing if the unit is already mid-attack-move toward the same
  destination tile (avoids replanning every frame at chokepoints).

### Extract a shared, parameterized function
Refactor the body into a free function `Unit.h`/`Unit.cpp` (or a new small header if that fits
project conventions better — check whether `AICommander.h` is the only includer of anything
Retreat-related, in which case a shared location under `units/` is cleaner):
```cpp
// Shared by AICommander::RetreatTick and the player-facing equivalent.
// Orders any teamID-matching, non-Engineer, alive unit below
// kRetreatHealthFraction of its type's max HP to fall back toward `home`
// via a fighting withdrawal (attack-move, not plain move — see the
// original AICommander comment on why plain move regressed soak outcomes).
void RetreatIfLowHP(Registry &registry, TileMap &map, OccupancyGrid *occ, Vector2 home,
                    int teamID, float healthFraction);
```
Hoist the magic `0.3f` into a shared `constexpr float kRetreatHealthFraction = 0.3f;` (currently
inline in `AICommander.cpp`) so both call sites reference one named constant, but keep it as a
*parameter* to `RetreatIfLowHP` (not hardcoded inside) so the player-facing feature can expose a
configurable threshold later without touching the AI's call site.

Refactor `AICommander::RetreatTick` to delegate to this new function, passing `homeTile_`-derived
`home`, `teamID_`, and `kRetreatHealthFraction`. Verify with the existing AI soak tests
(`skirmish_tests.cpp`) that behavior is bit-identical after the refactor before adding anything new
— this refactor alone should be a zero-behavior-change commit, easy to verify in isolation.

### Player-facing addition
There's no per-player "commander" object equivalent to `AICommander` — the player is driven
directly through `Game`. Add:
1. A toggle. Two reasonable scopes — pick one explicitly and document the choice:
   - **Per-unit** (`bool autoRetreat = false;` on `Unit`, near `stance`) — lets players opt
     individual units in/out (e.g. keep glass-cannon artillery on auto-retreat, leave tanks
     manually controlled). More flexible, slightly more UI surface (needs a per-unit toggle
     control, e.g. a keybinding that flips the flag on the current selection).
   - **Global** (a single `bool playerAutoRetreat = false;` on `Game`) — simpler, one hotkey,
     matches "auto-retreat" as a blanket stance rather than a per-unit setting.
   Recommend **per-unit**, since it composes with `Stance` (`Unit.h:72-77`) more naturally and
   avoids surprising a player who wants some units to hold and die rather than retreat
   (Artillery/turtling comps). If per-unit, gate `RetreatIfLowHP`'s internal scan on
   `unit.autoRetreat` in addition to team/type/health.
2. A per-frame call site in `Game.cpp`'s main update loop, alongside the other per-frame economy/
   production ticks (near wherever `queue.Update`/`nodes.GatherTick` are called): `RetreatIfLowHP(
   registry, map, &occ, retreatDestination, playerTeamID, kRetreatHealthFraction);`.
3. Destination: reuse whatever rally-point concept already exists for the player
   (`Game.h`'s `rallyPos`, set via right-click-while-`settingRally`) as the retreat destination, or
   fall back to the nearest owned `BuildingType::Base` if no rally point has been set. Implement
   the fallback explicitly (don't retreat to `{0,0}` if `rallyPos` is unset).

### Tests
New `tests/unit-tests/retreat_tests.cpp` (`RunRetreatTests`): construct a unit below threshold with
`autoRetreat=true` near an enemy, call `RetreatIfLowHP`, assert it gets an attack-move order toward
`home` (not a plain move — assert `unit.attackMove == true`). Construct one at full health, assert
no order is issued. Construct an `Engineer` below threshold, assert it's excluded. Re-run
`ai_commander_tests.cpp` and the soak tests after the `RetreatTick` refactor to confirm zero
behavior change for the AI path.

---

## 4. Auto-repair toggle on buildings (funds cap / % selector)

### Current model (buildings only heal via free Engineer channel)
`Building` (`src/game/public/economy/Building.h:32-45`) has `health`/`maxHealth` but **no
per-frame self-tick at all** — no `UpdateBuilding(dt)` exists. The only healing path today is the
Engineer repair channel (`Unit.cpp`'s repair branch), which is explicitly **free** ("costs time,
not resources," per a comment on `IssueRepairOrder`). This feature is a genuine departure: a
**funds-costing** self-repair, needing new plumbing end to end.

### Data model
Add to `Building`:
```cpp
bool autoRepairEnabled = false;
// Fraction of the building's *current* funds income (or a flat rate — pick
// one, see "cost model" below) allowed to be spent on auto-repair per
// second. 1.0 = uncapped (repair as fast as the flat per-HP cost allows).
float autoRepairFundsCapFraction = 1.0f;
```

### Cost model — decide explicitly, don't leave ambiguous
Two readings of "funds cap / % selector" from the feature text; pick one for v1 and document it:
- **(a) Cost-per-HP with a spend-rate cap**: add `constexpr float kRepairCostIronPerHP = ...;`
  (and oil if buildings should cost oil too — check `UnitCost`/`CostOf` in `UnitFactory.h` for the
  established iron/oil cost ratio convention for the unit types and mirror it), and
  `autoRepairFundsCapFraction` limits how much of the tick's cost can be spent per second (e.g. a
  slider from "repair slowly, cheaply" to "repair at full rate"). Recommend this reading — it maps
  cleanly onto a single slider UI control.
- **(b) HP-percentage target**: `autoRepairFundsCapFraction` instead means "auto-repair stops once
  the building reaches X% HP" (a target ceiling below 100%, e.g. to save funds by not fully
  topping off). This reads less naturally as "funds cap" from the original wording — only pick this
  if product clarifies that's the intent.

### New per-frame system function
Mirror `UpdateBaseIncome`'s exact signature shape (`Building.h:71-72`,
`Building.cpp:131-146`):
```cpp
// Ticks auto-repair for every Operational, autoRepairEnabled building of
// `teamID` (or all teams if -1, matching UpdateBaseIncome's convention):
// heals toward maxHealth at kRepairCostIronPerHP-derived rate, gated by
// ResourceSystem::TrySpend so repair pauses (not cancels) when funds run
// short, mirroring AICommander::MaintainProduction's
// "insufficient funds: retry next tick" idiom.
void UpdateBuildingAutoRepair(Registry &registry, ResourceSystem &resources, float dt,
                              int teamID = -1);
```
Add to `Building.h`/`Building.cpp`, called once per frame per owner from the same places
`UpdateBaseIncome` is already called (`Game.cpp` for the player, `AICommander.cpp:207-210`-ish
for AI, if AI should also get this — decide whether AI buildings should also auto-repair; simplest
consistent choice is yes, gated the same way).

**`TrySpend` is currently atomic (all-or-nothing)** — `ResourceSystem.h:14`. For a smooth
per-frame partial heal, either (a) accept the atomicity and only heal in discrete chunks large
enough to always afford or skip a frame's tick entirely when unaffordable (simplest, slightly
choppy at low funds), or (b) add a new `ResourceSystem` method for partial spend
(`TrySpendPartial(ironWanted, oilWanted) -> actuallySpent`) if smoothness matters — recommend (a)
for v1, it's simpler and the existing `MaintainProduction` precedent already accepts this kind of
choppiness ("insufficient funds: income will retry next tick").

### UI gap: buildings aren't selectable
No `Building::isSelected`, no `PickBuildingAt`, no building-specific HUD panel exists —
`DrawSelectionPanel` (`Hud.cpp:106-119`) only ever reads `SelectedUnit`. Building a full
click-to-select-building system is out of scope for this feature alone (see roadmap P4).
**Recommended v1 scope**: a **global** player-side toggle + slider (on `Game`, e.g. `bool
autoRepairEnabled` + `float autoRepairFundsCapFraction`, applied uniformly to every owned building
via the `teamID` parameter above) rather than true per-building state — matches how this kind of
toggle is often implemented as a blanket player preference in other RTS QoL patches, and sidesteps
needing building selection at all. If a later pass adds building selection, promote the global
toggle to a per-building override at that point.

### Tests
New block in `building_tests.cpp`: a damaged, `Operational`, `autoRepairEnabled` building with
sufficient funds; call `UpdateBuildingAutoRepair`; assert `health` increased and `resources` was
debited by the expected amount. Repeat with insufficient funds; assert `health` unchanged and
`resources` untouched (skip, not partial-charge, per the (a) decision above). Assert a
non-`autoRepairEnabled` or non-`Operational` building is untouched.
