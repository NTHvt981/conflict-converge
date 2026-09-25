# Unit Automation

Shipped: fully shipped 2026-09-17, with the caveats below intact.

Overkill protection, target priority, opt-in fighting withdrawal, and a
per-Engineer auto-repair toggle. AI auto-repair is a deliberate
non-goal (economy soak-tuned without it).

## Behavior

- Overkill protection: reserved-damage map (WindUp/Recover power sums)
  steers `AcquireTarget` away from already-doomed targets. Buildings
  unaffected.
- Target priority: armor-hunters (vehicle hulls, AntiArmorInfantry) weight
  vehicle targets 1.5x in the threat score.
- Auto-retreat: opted-in player units withdraw at 30% HP
  (`RetreatIfLowHP`, home = rally or nearest owned Base centroid);
  AI retreats on its own path (`RetreatTick`).
- Building auto-repair: per-Engineer `Orders::autoRepair` toggle —
  idle Engineers acquire the nearest damaged same-team vehicle or
  Operational building in 256px and repair it through the channeled
  order path; Engineers still repair free.

## Key files

- `src/game/public/units/Targeting.h` + `private/units/Targeting.cpp` —
  priority weights, reserved-damage skip, threat wiring.
- `src/game/public+private/units/Unit.{h,cpp}` — `autoRetreat` flag,
  retreat helpers, reserved-map build.
- `src/game/public/units/Extensions.h` — `Orders::autoRepair` toggle.
- `src/game/public+private/units/UnitCommands.{h,cpp}` —
  `ToggleSelectionAutoRepair` selection command.
- `src/game/private/core/Simulation.cpp` — player-only
  auto-retreat tick (the AI-unwired proof).
- Tests: `Targeting` (priority/overkill), `Retreat`,
  `AutoRepair` (toggle/acquire), `Simulation` (wiring) suites.

## Decisions

- Repair is a per-Engineer toggle driving the channeled order path;
  idle togglers acquire the nearest valid patient in range.
- Retreat threshold is a hardcoded 30% constant (no UI); origin anchor
  semantics stay top-left.
- Open (explicit non-goals): AI auto-repair, per-building overrides,
  retreat-threshold UI.
