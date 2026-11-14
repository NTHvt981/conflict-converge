// Unit tests for the Vfx subsystem's particle and damage-number pools.

#include "test_harness.h"

#include "app/ui/Vfx.h"

void RunVfxTests()
{
	// --- particle pool: spawn, expiry, clear ---
	{
		Vfx fx;
		CC_CHECK(fx.ParticleCount() == 0);
		fx.SpawnBurst({ 100.0f, 100.0f }, RED, 10, 60.0f, 1.0f);
		CC_CHECK(fx.ParticleCount() == 10);
		fx.Update(0.5f);
		CC_CHECK(fx.ParticleCount() == 10); // half-life: all still alive
		fx.Update(0.6f);
		CC_CHECK(fx.ParticleCount() == 0); // expired and compacted
		fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 5, 10.0f, 1.0f);
		fx.Clear();
		CC_CHECK(fx.ParticleCount() == 0);
	}

	// --- pool motion keeps bursts alive while life remains ---
	{
		Vfx fx;
		fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 20, 100.0f, 10.0f);
		fx.Update(1.0f);
		CC_CHECK(fx.ParticleCount() == 20);
	}

	// --- pool cap: bursts beyond kMaxParticles are dropped, never grow ---
	{
		Vfx fx;
		for (int i = 0; i < 40; ++i)
		{
			fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 50, 10.0f, 10.0f);
		}
		CC_CHECK(fx.ParticleCount() == Vfx::kMaxParticles);
		fx.Update(11.0f);
		CC_CHECK(fx.ParticleCount() == 0);
	}

	// --- damage numbers: spawn, rise, expiry, clear ---
	{
		Vfx dn;
		CC_CHECK(dn.DamageNumberCount() == 0);
		dn.SpawnDamageNumber({ 100.0f, 100.0f }, 25.0f);
		CC_CHECK(dn.DamageNumberCount() == 1);
		dn.Update(Vfx::kDamageNumberLife / 2.0f);
		CC_CHECK(dn.DamageNumberCount() == 1); // half-life: still alive (and risen)
		dn.Update(Vfx::kDamageNumberLife);
		CC_CHECK(dn.DamageNumberCount() == 0); // expired and compacted
		dn.SpawnDamageNumber({ 0.0f, 0.0f }, 1.0f);
		dn.Clear();
		CC_CHECK(dn.DamageNumberCount() == 0);
	}

	// --- damage pool cap: spawns beyond kMaxDamageNumbers are dropped, never grow ---
	{
		Vfx dn;
		for (int i = 0; i < static_cast<int>(Vfx::kMaxDamageNumbers) + 10; ++i)
		{
			dn.SpawnDamageNumber({ 0.0f, 0.0f }, 1.0f);
		}
		CC_CHECK(dn.DamageNumberCount() == Vfx::kMaxDamageNumbers);
		dn.Update(Vfx::kDamageNumberLife + 1.0f);
		CC_CHECK(dn.DamageNumberCount() == 0);
	}

	// --- zero-count and zero-life bursts are safe ---
	{
		Vfx fx;
		fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 0, 10.0f, 1.0f);
		CC_CHECK(fx.ParticleCount() == 0);
		fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 3, 10.0f, 0.0f);
		CC_CHECK(fx.ParticleCount() == 3); // maxLife guards the divide
		fx.Update(0.01f);
		CC_CHECK(fx.ParticleCount() == 0);
	}

	// --- ResetForMatch clears both pools ---
	{
		Vfx fx;
		fx.SpawnBurst({ 0.0f, 0.0f }, WHITE, 4, 10.0f, 1.0f);
		fx.SpawnDamageNumber({ 0.0f, 0.0f }, 5.0f);
		fx.ResetForMatch();
		CC_CHECK(fx.ParticleCount() == 0);
		CC_CHECK(fx.DamageNumberCount() == 0);
	}
}
