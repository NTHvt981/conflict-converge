#include "economy/ResourceSystem.h"

void ResourceSystem::AddIron(long amount)
{
	iron += amount;
}

void ResourceSystem::AddOil(long amount)
{
	oil += amount;
}

bool ResourceSystem::TrySpend(long ironCost, long oilCost)
{
	if (ironCost < 0 || oilCost < 0)
	{
		return false;
	}
	if (iron < ironCost || oil < oilCost)
	{
		return false;
	}
	iron -= ironCost;
	oil -= oilCost;
	return true;
}

void ResourceSystem::TickIncome(float ironPerSecond, float oilPerSecond, float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}
	if (ironPerSecond < 0.0f)
	{
		ironPerSecond = 0.0f;
	}
	if (oilPerSecond < 0.0f)
	{
		oilPerSecond = 0.0f;
	}
	ironCarry_ += ironPerSecond * dt;
	oilCarry_ += oilPerSecond * dt;
	const long ironWhole = static_cast<long>(ironCarry_);
	const long oilWhole = static_cast<long>(oilCarry_);
	iron += ironWhole;
	oil += oilWhole;
	ironCarry_ -= static_cast<float>(ironWhole);
	oilCarry_ -= static_cast<float>(oilWhole);
}

void ResourceSystem::TickIncomeCapped(float ironPerSecond, float oilPerSecond, float dt,
									  long ironCap, long oilCap)
{
	if (dt <= 0.0f)
	{
		return;
	}
	if (ironPerSecond < 0.0f)
	{
		ironPerSecond = 0.0f;
	}
	if (oilPerSecond < 0.0f)
	{
		oilPerSecond = 0.0f;
	}
	ironCarry_ += ironPerSecond * dt;
	oilCarry_ += oilPerSecond * dt;
	long ironWhole = static_cast<long>(ironCarry_);
	long oilWhole = static_cast<long>(oilCarry_);
	if (iron + ironWhole > ironCap)
	{
		ironWhole = ironCap > iron ? ironCap - iron : 0;
	}
	if (oil + oilWhole > oilCap)
	{
		oilWhole = oilCap > oil ? oilCap - oil : 0;
	}
	iron += ironWhole;
	oil += oilWhole;
	ironCarry_ -= static_cast<float>(ironWhole);
	oilCarry_ -= static_cast<float>(oilWhole);
	if (ironCarry_ > static_cast<float>(ironCap))
	{
		ironCarry_ = static_cast<float>(ironCap);
	}
	if (oilCarry_ > static_cast<float>(oilCap))
	{
		oilCarry_ = static_cast<float>(oilCap);
	}
}

float ResourceSystem::IronCarry() const
{
	return ironCarry_;
}

float ResourceSystem::OilCarry() const
{
	return oilCarry_;
}

void ResourceSystem::SetCarry(float ironCarry, float oilCarry)
{
	ironCarry_ = ironCarry;
	oilCarry_ = oilCarry;
}
