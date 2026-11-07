#include "economy/Nodes.h"

#include "world/TileMap.h"

bool ResourceNode::IsDepleted() const
{
	return amount <= 0.0f;
}

bool ResourceNodes::SpawnNode(const TileMap &map, ResourceKind kind, cc::IVec2 tile,
							  float amount, float respawnDelay)
{
	if (amount <= 0.0f || !map.InBounds(tile) ||
		map.Get(tile) != TerrainType::Grass || FindAt(tile) != nullptr)
	{
		return false;
	}
	ResourceNode node;
	node.kind = kind;
	node.tile = tile;
	node.amount = amount;
	node.maxAmount = amount;
	node.respawnDelay = respawnDelay;
	node.respawnTimer = 0.0f;
	nodes_.push_back(node);
	return true;
}

void ResourceNodes::Update(float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}
	for (ResourceNode &node : nodes_)
	{
		if (!node.IsDepleted() || node.respawnDelay < 0.0f)
		{
			continue;
		}
		node.respawnTimer -= dt;
		if (node.respawnTimer <= 0.0f)
		{
			node.amount = node.maxAmount;
			node.respawnTimer = 0.0f;
		}
	}
}

std::size_t ResourceNodes::Count() const
{
	return nodes_.size();
}

const ResourceNode *ResourceNodes::FindAt(cc::IVec2 tile) const
{
	for (const ResourceNode &node : nodes_)
	{
		if (node.tile == tile)
		{
			return &node;
		}
	}
	return nullptr;
}

void ResourceNodes::Each(const std::function<void(const ResourceNode &)> &fn) const
{
	for (const ResourceNode &node : nodes_)
	{
		fn(node);
	}
}

void ResourceNodes::RestoreNode(ResourceKind kind, cc::IVec2 tile, float amount, float maxAmount,
								float respawnDelay, float respawnTimer)
{
	ResourceNode node;
	node.kind = kind;
	node.tile = tile;
	node.amount = amount;
	node.maxAmount = maxAmount;
	node.respawnDelay = respawnDelay;
	node.respawnTimer = respawnTimer;
	nodes_.push_back(node);
}

float ResourceNodes::IronCarry() const
{
	return ironCarry_;
}

float ResourceNodes::OilCarry() const
{
	return oilCarry_;
}

void ResourceNodes::SetCarry(float ironCarry, float oilCarry)
{
	ironCarry_ = ironCarry;
	oilCarry_ = oilCarry;
}
