# Conflict Converge — Balance Sheet (M13)

Mirror factions (Q82): both sides share one stat table, so balance is about
unit roles and pacing, not faction asymmetry. All values from
`UnitStats.cpp` (`BaseStats`), `UnitFactory.cpp` (`CostOf`),
`Production.cpp` (`BuildTime = max(2s, 2 + iron/50)`), `Combat.cpp`
(damage matrix), `Building.cpp` (structure HP).

## Units

| Unit | Cost I/O | Build | HP | Armor | Dmg | Pow | Rng | CD | Spd | Sight | Role |
|------|----------|-------|----|-------|-----|-----|-----|----|-----|-------|------|
| Infantry | 25/0 | 2.5s | 100 | RUBBER | KINETIC | 10 | 128 | 1.0s | 64 | 320 | cheap line holder, scout (1.5x fog sight) |
| AntiArmor | 30/5 | 2.6s | 90 | RUBBER | EXPLOSIVE | 25 | 128 | 1.5s | 64 | 320 | tank hunter (1.25x vs STEEL) |
| Engineer | 40/0 | 2.8s | 60 | RUBBER | KINETIC | 2 | 64 | 1.0s | 64 | 256 | harvest + repair (15 HP/s, time only) |
| IFV | 80/20 | 3.6s | 200 | STEEL | KINETIC | 15 | 192 | 0.8s | 128 | 384 | fast scout/skirmisher (1.5x fog sight) |
| Artillery | 120/40 | 4.4s | 150 | STEEL | EXPLOSIVE | 40 | 384 | 3.0s | 48 | 320 | siege; blind-fires into shroud, no penalty |
| LightTank | 150/50 | 5.0s | 300 | STEEL | KINETIC | 20 | 192 | 1.2s | 96 | 320 | main battle tank |
| HeavyTank | 250/100 | 7.0s | 500 | COMPOSITE | EXPLOSIVE | 35 | 224 | 1.8s | 64 | 320 | breakthrough (resists EXPLOSIVE 0.75x) |

## Damage matrix (attacker row x defender column)

| | STEEL | RUBBER | COMPOSITE |
|---|---|---|---|
| KINETIC | 1.00 | 0.75 | 0.50 |
| EXPLOSIVE | 1.25 | 1.00 | 0.75 |
| ENERGY | 0.50 | 1.25 | 1.00 |

(ENERGY has no unit yet — reserved for future/tech.)

## Key TTKs (seconds, duel, no maneuver)

- Infantry vs Infantry: ~14s. LightTank vs Infantry: ~8s.
- AntiArmor vs LightTank: ~15s. AntiArmor vs HeavyTank: ~40s (needs focus fire).
- LightTank vs LightTank: ~18s. HeavyTank vs HeavyTank: ~36s.
- Artillery vs LightTank: ~18s (outranges 384 vs 192 — fires unanswered).

## Structures & economy

| Building | Footprint | HP |
|----------|-----------|----|
| Base | 2x2 | 400 (base income: 2 iron/s + 1 oil/s) |
| Depot | 1x1 | 200 |
| Factory | 2x2 | 350 |

Harvest: 10 iron/s or 8 oil/s per Engineer on a live node. Starting funds:
1000 iron / 500 oil (both sides, fair rules).

## AI waves (M8)

| Difficulty | Harvesters | Threshold | Scout | Cooldown | Comp |
|------------|-----------|-----------|-------|----------|------|
| Easy | 1 | 3 | 60s | 30s | Infantry |
| Medium | 2 | 5 | 30s | 20s | Infantry x2 + LightTank |
| Hard | 3 | 4 | 15s | 15s | Infantry + LightTank + Artillery + IFV + retreats |

## Pass 1 (M13): soak results

- Easy-vs-Medium AI skirmish (16x12 mirror arena, fair funds): Medium wins
  decisively in 4916 frames (~82 sim-seconds); no stalemate. Kill telemetry
  showed the pre-tune equilibrium (44-17 kills over 3 min, counts flat:
  serial production out-replaced attrition), so the tune targeted the real
  terminator — demolition: Base 600→400, Factory 500→350, Depot 250→200
  (a LightTank levels a Base in ~20s now). Unit stats untouched.
- Drivers of decisiveness, in order: attack-move waves (plain move orders
  walked past defenders — measured walk-through stalemate), destructible
  production (razed AI stays down via factory-gated queues), fair-rules
  harvest routing (AI crews used to feed the player pool).
- Retune if playtesting disagrees; the soak asserts termination + Medium win.
