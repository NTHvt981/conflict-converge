# Add unit behavior

## Goal

Teach units a genuinely new pattern of acting — an order type, a
channel, a movement habit, a seat rule — where no existing switch
covers it. Behavior lives in one module per category; the work is
picking the category, imitating its in-tree exemplar, and giving the
pattern a frame tick and a suite.

Because your behavior is unknown, this recipe is a map, not steps that
guess your semantics: the ownership table below resolves to files,
each with one working exemplar to imitate.

## Prerequisites

A Debug build that compiles and passes the test suite.

## The ownership map

| Behavior category | Owning code | Exemplar to imitate |
|---|---|---|
| channeled orders (repair, heal) | `src/game/private/units/UnitRepair.cpp` + issue path in `UnitOrders.cpp` | Medic heal: `HealAim`/`CanHealTarget`, `IssueHealOrder` |
| targeting & overkill | `src/game/private/units/Targeting.cpp`, `src/game/private/units/Combat.cpp` | reserved-damage map steering target acquisition |
| attack timing | `src/game/private/units/UnitCombat.cpp` + `AttackPhase` in `units/Unit.h` combat state | WindUp/Recover phases in the attack driver |
| movement & pathing | `src/game/private/units/UnitMovement.cpp`, `src/game/public/world/Pathfinder.h` | locomotor-driven path costs and the step toward goal |
| transport seats | `src/game/private/units/UnitTransport.cpp` | IFV cargo manifest with the boarding gestures |
| orders & gating | `src/game/private/units/UnitOrders.cpp` | halt + rally semantics in the order driver |
| selection commands & UI affordances | `src/game/private/units/UnitCommands.cpp` + `src/game/private/core/PlayingInputOrders.cpp` | `ToggleSelectionAutoRepair`, cursor intent |
| AI usage | `src/game/private/units/AICommanderIntel.cpp` / `AICommanderEconomy.cpp` / `AICommanderTeams.cpp` | `HarassWantsType` in `AICommanderTeams.cpp` + composition filter in `data/configs/ai.json` |
| production/economy hooks | `src/game/private/economy/Production.cpp`, `Building.cpp` | construction ramp into `Operational` |

## Steps

1. **Locate the tick.** If the behavior needs work every frame, it is
   invoked from `Simulation::Step`'s call list in
   `src/game/private/core/Simulation.cpp`. Match the phase to intent:
   movement runs before the death reaping, economy edges before
   production, commanders after retreat — place the new call among
   neighbors it interacts with, never a frame apart.

2. **Write the pattern as a helper in the owning TU.** Keep it
   file-local (`static` / anonymous namespace) while only that TU
   calls in; promote onto a header when a caller in another module
   needs it. Unit state rides `Unit` / `Extensions` fields — do not
   add globals.

3. **Guard the gate.** Behavior only applies to valid pairs: reuse the
   existing guards' shape (`type == UnitType::...`, team checks, range
   checks on the unit's own stats) rather than inventing new state.

4. **Give the UI its affordances.** Cursor intent comes from
   `src/game/private/app/ui/Cursor.cpp` reading the same predicate the
   order code validates, so the cursor cannot lie about what the
   right-click will do.

5. **Learn how the AI meets it.** A behavior with no commander path is
   player-only by construction; make that visible: the AI reads the
   per-type switches, so new units default out of harass compositions
   until a team file speaks for them.

6. **Test it headlessly.** Drive `Simulation::Step` on a registry with
   units spawned through the unit factory; assert on the resulting
   order, state, or damage. Visual behavior is verified by launching
   the game, which is exactly what the standing rules prescribe for
   draw-side work.
