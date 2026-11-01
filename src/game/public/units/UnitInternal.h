
#pragma once

#include "units/Unit.h"

#include "core/MathUtils.h"
#include "units/Extensions.h"

// Shared by the Unit TUs (Unit.cpp, UnitOrders, UnitCombat, UnitRepair,
// UnitMovement, UnitTransport) and the Targeting/Combat scans. Not general API:
// helpers that two or more Unit TUs call but no other subsystem needs stay off
// Unit.h.
void LoseTarget(Unit &unit, CombatState &combat);
void StopMoving(Unit &unit, Mover &mover);
void OnOrderFinished(Unit &unit, const OrderContext &ctx);
void OnOrderCancelled(Orders &orders);
bool TurretAimed(Registry &registry, Entity self, Vector2 fromPos, Vector2 aimPos,
				 float dtSeconds);
bool RepairAim(const Registry &registry, const Unit &engineer, Entity target, Vector2 &outPos);
inline bool BetterCandidate(Entity candidateId, Vector2 candidatePos, float candidateDist,
							Entity bestId, Vector2 bestPos, float bestDist)
{
	if (bestId == kInvalidEntity)
	{
		return candidateDist < bestDist;
	}
	if (candidateDist < bestDist)
	{
		return true;
	}
	if (candidateDist > bestDist)
	{
		return false;
	}
	const cc::IVec2 candidateTile = cc::WorldToTile(cc::ToGlm(candidatePos));
	const cc::IVec2 bestTile = cc::WorldToTile(cc::ToGlm(bestPos));
	if (candidateTile.y != bestTile.y)
	{
		return candidateTile.y < bestTile.y;
	}
	if (candidateTile.x != bestTile.x)
	{
		return candidateTile.x < bestTile.x;
	}
	return candidateId < bestId;
}

void AcquireAutoRepair(Registry &registry, Entity self, const Unit &engineer, Orders &orders);
bool HealAim(const Registry &registry, const Unit &medic, Entity target, Vector2 &outPos);
cc::IVec2 RepairApproachTile(const TileMap &map, cc::IVec2 aimTile);
bool ValidateTarget(Registry &registry, const Unit &seeker, Entity id, const FogOfWar *fog);
Vector2 TargetPosition(Registry &registry, Entity id);
bool EngageTarget(Entity self, Unit &attacker, CombatState &combat, Registry &registry,
				  TileMap &map, Entity id, const FogOfWar *fog, float dtSeconds);

template <typename LandHit>
void UpdateAttackPhases(Unit &attacker, CombatState &combat, float dtSeconds, LandHit landHit)
{

	if (combat.phase == AttackPhase::Ready)
	{
		if (combat.cooldown <= 0.0f && attacker.attackPower > 0)
		{
			combat.phase = AttackPhase::WindUp;
			combat.phaseTime = attacker.windupTime;
		}
		return;
	}
	if (combat.phase == AttackPhase::WindUp)
	{
		combat.phaseTime -= dtSeconds;
		if (combat.phaseTime <= 0.0f)
		{
			landHit();
			combat.phase = AttackPhase::Recover;
		}
		return;
	}
	if (combat.cooldown <= 0.0f)
	{
		combat.phase = AttackPhase::Ready;
	}
}
