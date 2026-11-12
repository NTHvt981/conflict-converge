#include "app/data/ArtDraw.h"

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
		{ 0.80f, { { -28, 0 }, { 28, 0 } } },
		{ 0.70f, { { 0, -28 }, { -26, 20 }, { 26, 20 } } },
		{ 0.65f, { { -26, -26 }, { 26, -26 }, { -26, 26 }, { 26, 26 } } },
		{ 0.55f, { { 0, 0 }, { -30, -30 }, { 30, -30 }, { -30, 30 }, { 30, 30 } } },
		{ 0.50f,
		  { { -36, -34 }, { 0, -34 }, { 36, -34 }, { -36, 34 }, { 0, 34 }, { 36, 34 } } },
	};
	const SquadPreset &preset = kPresets[count - 1];
	layout.count = count;
	layout.scale = preset.scale;

	const unsigned int h = HashId(id);
	const float jitterPx = 4.0f * preset.scale;
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

void ArtDraw::DrawUnit(const ArtSprites &sprites, UnitType type, TeamId teamID,
					   UnitFrame frame, Vector2 tileCorner) const
{
	if (sprites.UseRectangles())
	{
		return;
	}
	const Texture2D &tex = sprites.units_[static_cast<int>(type)][ArtSprites::TeamSlot(teamID)][static_cast<int>(frame)];
	DrawTextureV(tex, { tileCorner.x + cc::kQuarterTile, tileCorner.y + cc::kQuarterTile },
				 WHITE);
}

void ArtDraw::DrawAtlasFrame(const ArtSprites &sprites, const std::string &spriteName,
							 Vector2 tileCorner, Color tint, float scale,
							 float rotationDeg) const
{
	if (!sprites.UseAtlas() || spriteName.empty())
	{
		return;
	}
	const SpriteDefInfo *s = FindSpriteByName(sprites.sheet_, spriteName);
	if (s == nullptr)
	{
		return;
	}
	if (s->maskSprite == 0)
	{
		DrawOneAtlasSprite(sprites, *s, tileCorner, tint, scale, rotationDeg);
		return;
	}
	DrawOneAtlasSprite(sprites, *s, tileCorner, WHITE, scale, rotationDeg);
	if (const SpriteDefInfo *mask = FindSpriteById(sprites.sheet_, s->maskSprite);
		mask != nullptr)
	{
		DrawOneAtlasSprite(sprites, *mask, tileCorner, tint, scale, rotationDeg);
	}
}

bool ArtDraw::SpriteDrawRect(const ArtSprites &sprites, const std::string &spriteName,
							 Vector2 tileCorner, Rectangle &out, float scale) const
{
	if (!sprites.UseAtlas() || spriteName.empty())
	{
		return false;
	}
	const SpriteDefInfo *s = FindSpriteByName(sprites.sheet_, spriteName);
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

void ArtDraw::DrawOneAtlasSprite(const ArtSprites &sprites, const SpriteDefInfo &s,
								 Vector2 tileCorner, Color tint, float scale,
								 float rotationDeg) const
{
	const auto it = sprites.atlas_.find(s.texture);
	if (it == sprites.atlas_.end() || it->second.id == 0)
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

void ArtDraw::DrawBuilding(const ArtSprites &sprites, BuildingType type, TeamId teamID,
						   int tileX, int tileY, BuildingFrame frame, Color tint) const
{
	if (sprites.UseRectangles())
	{
		return;
	}
	const Vector2 corner = cc::ToRaylib(cc::TileToWorld(tileX, tileY));
	const int t = static_cast<int>(type);
	const int f = static_cast<int>(frame);
	const ArtSprites::BuildingFrameArt &art =
		sprites.buildingArt_[t].hasFrame[f] ? sprites.buildingArt_[t].frames[f]
											: sprites.buildingArt_[t].frames[0];
	if (art.mask.id != 0)
	{
		DrawTextureV(art.base, corner, tint);
		DrawTextureV(art.mask, corner, MultiplyColor(sprites.TeamTint(teamID), tint));
		return;
	}
	DrawTextureV(art.flat[ArtSprites::TeamSlot(teamID)], corner, tint);
}

void ArtDraw::DrawTerrain(const ArtSprites &sprites, TerrainType type, Vector2 corner) const
{
	const int slot = ArtSprites::TerrainSlot(type);
	if (sprites.UseRectangles() || slot < 0)
	{
		return;
	}
	DrawTextureV(sprites.terrain_[slot], corner, WHITE);
}

bool ArtDraw::HasFont(const ArtSprites &sprites) const
{
	return sprites.HasFont();
}

const Font &ArtDraw::UiFont(const ArtSprites &sprites) const
{
	return sprites.UiFont();
}

void ArtDraw::DrawUiText(const ArtSprites *sprites, const char *text, int x, int y, int size,
						 Color color)
{
	if (sprites != nullptr && sprites->HasFont())
	{
		DrawTextEx(sprites->UiFont(), text, { static_cast<float>(x), static_cast<float>(y) },
				   static_cast<float>(size), static_cast<float>(size) / 10.0f, color);
		return;
	}
	DrawText(text, x, y, size, color);
}

void ArtDraw::DrawNode(const ArtSprites &sprites, ResourceKind kind, Vector2 center) const
{
	if (sprites.UseRectangles())
	{
		return;
	}
	const Texture2D tex = sprites.nodes_[static_cast<int>(kind)];
	DrawTextureV(tex, { center.x - cc::kHalfTile, center.y - cc::kHalfTile }, WHITE);
}

void ArtDraw::DrawIcon(const ArtSprites &sprites, ResourceKind kind, Vector2 screenPos) const
{
	if (sprites.UseRectangles())
	{
		return;
	}
	DrawTextureV(sprites.icons_[static_cast<int>(kind)], screenPos, WHITE);
}

void ArtDraw::DrawRallyMarker(const ArtSprites &sprites, TeamId teamID,
							  Vector2 worldCenter) const
{
	if (sprites.UseRectangles() || sprites.rallyBase_.id == 0)
	{
		return;
	}
	const Vector2 corner = { worldCenter.x - cc::kQuarterTile,
							 worldCenter.y - cc::kQuarterTile };
	DrawTextureV(sprites.rallyBase_, corner, WHITE);
	DrawTextureV(sprites.rallyMask_, corner, sprites.TeamTint(teamID));
}
