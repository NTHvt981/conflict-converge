# Unit Automation

Shipped: fully shipped 2026-09-17, with the caveats below intact.

Overkill protection, target priority, opt-in fighting withdrawal, and a
global building auto-repair toggle. AI auto-repair is a deliberate
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
- Building auto-repair: global toggle + rate slider, player team only —
  15 HP/s at 0.5 iron/HP in atomic chunks (pauses when broke, never
  partial), with fractional carry; Engineers still repair free.

## Key files

- `src/game/public/units/Targeting.h` + `private/units/Targeting.cpp` —
  priority weights, reserved-damage skip, threat wiring.
- `src/game/public+private/units/Unit.{h,cpp}` — `autoRetreat` flag,
  retreat helpers, reserved-map build.
- `src/game/public+private/economy/Building.{h,cpp}` —
  `UpdateBuildingAutoRepair` (iron-only flat rate).
- `src/game/private/core/Simulation.cpp` — player-only auto-repair and
  auto-retreat ticks (the AI-unwired proof).
- Tests: `Targeting` (priority/overkill), `Retreat`, `Building` (repair),
  `Simulation` (wiring) suites.

## Decisions

- Repair is a global toggle, not per-building; cost is flat iron-only
  (no partial-spend path added).
- Retreat threshold is a hardcoded 30% constant (no UI); origin anchor
  semantics stay top-left.
- Open (explicit non-goals): AI auto-repair, per-building overrides,
  retreat-threshold UI.
