#include "app/data/Art.h"

#include "core/MathUtils.h"

UnitFrame FrameForPhase(AttackPhase phase)
{
	switch (phase)
	{
	case AttackPhase::WindUp:
	case AttackPhase::Recover:
		return UnitFrame::Attack;
	case AttackPhase::Ready:
		break;
	}
	return UnitFrame::Idle;
}

static unsigned int HashId(unsigned int id)
{
	id = ((id >> 16) ^ id) * 0x45d9f3b;
	id = ((id >> 16) ^ id) * 0x45d9f3b;
	id = (id >> 16) ^ id;
	return id;
}

SquadLayout SquadSlots(UnitType type, unsigned int id, float healthFraction)
{
	SquadLayout layout;
	if (type != UnitType::RifleInfantry && type != UnitType::AntiArmorInfantry &&
		type != UnitType::PrototypeInfantry && type != UnitType::Medic)
	{
		layout.count = 1;
		return layout;
	}

	int count;
	if (type == UnitType::RifleInfantry || type == UnitType::PrototypeInfantry)
	{
		if (healthFraction > 0.80f)
			count = 5;
		else if (healthFraction > 0.60f)
			count = 4;
		else if (healthFraction > 0.40f)
			count = 3;
		else if (healthFraction > 0.20f)
			count = 2;
		else
			count = 1;
	}
	else
	{
		count = (healthFraction > 0.50f) ? 2 : 1;
	}

	struct SquadPreset
	{
		float scale;
		float offsets[6][2];
	};
	static constexpr SquadPreset kPresets[6] = {
		{ 1.00f, { { 0, 0 } } },
		{ 0.80f, { { -14, 0 }, { 14, 0 } } },
		{ 0.70f, { { 0, -14 }, { -13, 10 }, { 13, 10 } } },
		{ 0.65f, { { -13, -13 }, { 13, -13 }, { -13, 13 }, { 13, 13 } } },
		{ 0.55f, { { 0, 0 }, { -15, -15 }, { 15, -15 }, { -15, 15 }, { 15, 15 } } },
		{ 0.50f,
		  { { -18, -17 }, { 0, -17 }, { 18, -17 }, { -18, 17 }, { 0, 17 }, { 18, 17 } } },
	};
	const SquadPreset &preset = kPresets[count - 1];
	layout.count = count;
	layout.scale = preset.scale;

	const unsigned int h = HashId(id);
	const float jitterPx = 2.0f * preset.scale;
	for (int i = 0; i < count; ++i)
	{
		const unsigned int jitter = HashId(h + static_cast<unsigned int>(i));
		const float jx = (static_cast<float>(jitter & 0x7) / 7.0f - 0.5f) * 2.0f * jitterPx;
		const float jy =
			(static_cast<float>((jitter >> 3) & 0x7) / 7.0f - 0.5f) * 2.0f * jitterPx;
		layout.slots[i] = { preset.offsets[i][0] + jx, preset.offsets[i][1] + jy };
	}
	return layout;
}

namespace
{

Color MultiplyColor(Color a, Color b)
{
	return { static_cast<unsigned char>(a.r * b.r / 255),
			 static_cast<unsigned char>(a.g * b.g / 255),
			 static_cast<unsigned char>(a.b * b.b / 255),
			 static_cast<unsigned char>(a.a * b.a / 255) };
}

}

void Art::DrawUnit(UnitType type, TeamId teamID, UnitFrame frame, Vector2 tileCorner) const
{
	if (fallback_)
	{
		return;
	}
	const Texture2D tex = units_[static_cast<int>(type)][TeamSlot(teamID)][static_cast<int>(frame)];
	DrawTextureV(tex, { tileCorner.x + 16.0f, tileCorner.y + 16.0f }, WHITE);
}

void Art::DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint,
						 float scale, float rotationDeg) const
{
	if (!atlasReady_ || spriteName.empty())
	{
		return;
	}
	const SpriteDefInfo *s = FindSpriteByName(sheet_, spriteName);
	if (s == nullptr)
	{
		return;
	}
	if (s->maskSprite == 0)
	{
		DrawOneAtlasSprite(*s, tileCorner, tint, scale, rotationDeg);
		return;
	}
	DrawOneAtlasSprite(*s, tileCorner, WHITE, scale, rotationDeg);
	if (const SpriteDefInfo *mask = FindSpriteById(sheet_, s->maskSprite); mask != nullptr)
	{
		DrawOneAtlasSprite(*mask, tileCorner, tint, scale, rotationDeg);
	}
}

bool Art::SpriteDrawRect(const std::string &spriteName, Vector2 tileCorner, Rectangle &out,
						 float scale) const
{
	if (!atlasReady_ || spriteName.empty())
	{
		return false;
	}
	const SpriteDefInfo *s = FindSpriteByName(sheet_, spriteName);
	if (s == nullptr)
	{
		return false;
	}
	const float w = static_cast<float>(s->bounds.right - s->bounds.left);
	const float h = static_cast<float>(s->bounds.bottom - s->bounds.top);
	const Vector2 anchor = { tileCorner.x + cc::kHalfTile, tileCorner.y + cc::kHalfTile };
	out = { anchor.x - static_cast<float>(s->origin.x) * scale,
			anchor.y - static_cast<float>(s->origin.y) * scale, w * scale, h * scale };
	return true;
}

void Art::DrawOneAtlasSprite(const SpriteDefInfo &s, Vector2 tileCorner, Color tint,
							 float scale, float rotationDeg) const
{
	const auto it = atlas_.find(s.texture);
	if (it == atlas_.end() || it->second.id == 0)
	{
		return;
	}
	const float w = static_cast<float>(s.bounds.right - s.bounds.left);
	const float h = static_cast<float>(s.bounds.bottom - s.bounds.top);
	const Rectangle src = { static_cast<float>(s.bounds.left), static_cast<float>(s.bounds.top),
							w, h };
	const Vector2 anchor = { tileCorner.x + cc::kHalfTile, tileCorner.y + cc::kHalfTile };
	const Rectangle dest = { anchor.x - static_cast<float>(s.origin.x) * scale,
							 anchor.y - static_cast<float>(s.origin.y) * scale, w * scale,
							 h * scale };
	if (rotationDeg == 0.0f)
	{
		DrawTexturePro(it->second, src, dest, { 0.0f, 0.0f }, 0.0f, tint);
		return;
	}
	const Vector2 pivot = { dest.width / 2.0f, dest.height / 2.0f };
	DrawTexturePro(it->second, src, dest, pivot, rotationDeg, tint);
}

void Art::DrawBuilding(BuildingType type, TeamId teamID, int tileX, int tileY,
					   BuildingFrame frame, Color tint) const
{
	if (fallback_)
	{
		return;
	}
	const Vector2 corner = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
	const int t = static_cast<int>(type);
	const int f = static_cast<int>(frame);
	const BuildingFrameArt &art =
		buildingArt_[t].hasFrame[f] ? buildingArt_[t].frames[f] : buildingArt_[t].frames[0];
	if (art.mask.id != 0)
	{
		DrawTextureV(art.base, corner, tint);
		DrawTextureV(art.mask, corner, MultiplyColor(TeamTint(teamID), tint));
		return;
	}
	DrawTextureV(art.flat[TeamSlot(teamID)], corner, tint);
}

void Art::DrawTerrain(TerrainType type, Vector2 corner) const
{
	const int slot = TerrainSlot(type);
	if (fallback_ || slot < 0)
	{
		return;
	}
	DrawTextureV(terrain_[slot], corner, WHITE);
}

bool Art::HasFont() const
{
	return fontReady_;
}

const Font &Art::UiFont() const
{
	return uiFont_;
}

void Art::DrawUiText(const Art *art, const char *text, int x, int y, int size, Color color)
{
	if (art != nullptr && art->fontReady_)
	{
		DrawTextEx(art->uiFont_, text, { static_cast<float>(x), static_cast<float>(y) },
				   static_cast<float>(size), static_cast<float>(size) / 10.0f, color);
		return;
	}
	DrawText(text, x, y, size, color);
}

void Art::DrawNode(ResourceKind kind, Vector2 center) const
{
	if (fallback_)
	{
		return;
	}
	const Texture2D tex = nodes_[static_cast<int>(kind)];
	DrawTextureV(tex, { center.x - 16.0f, center.y - 16.0f }, WHITE);
}

void Art::DrawIcon(ResourceKind kind, Vector2 screenPos) const
{
	if (fallback_)
	{
		return;
	}
	DrawTextureV(icons_[static_cast<int>(kind)], screenPos, WHITE);
}

void Art::DrawRallyMarker(TeamId teamID, Vector2 worldCenter) const
{
	if (fallback_ || rallyBase_.id == 0)
	{
		return;
	}
	const Vector2 corner = { worldCenter.x - 8.0f, worldCenter.y - 8.0f };
	DrawTextureV(rallyBase_, corner, WHITE);
	DrawTextureV(rallyMask_, corner, TeamTint(teamID));
}
