#include "app/data/Art.h"

bool Art::Init(InitMode mode)
{
	return sprites_.Init(mode);
}

void Art::Shutdown()
{
	sprites_.Shutdown();
	particles_.Clear();
}

bool Art::IsReady() const
{
	return sprites_.IsReady();
}

bool Art::UseRectangles() const
{
	return sprites_.UseRectangles();
}

bool Art::UseAtlas() const
{
	return sprites_.UseAtlas();
}

std::expected<void, ConfigError> Art::LoadAtlas()
{
	return sprites_.LoadAtlas();
}

void Art::DrawUnit(UnitType type, TeamId teamID, UnitFrame frame, Vector2 tileCorner) const
{
	draw_.DrawUnit(sprites_, type, teamID, frame, tileCorner);
}

void Art::DrawAtlasFrame(const std::string &spriteName, Vector2 tileCorner, Color tint,
						 float scale, float rotationDeg) const
{
	draw_.DrawAtlasFrame(sprites_, spriteName, tileCorner, tint, scale, rotationDeg);
}

bool Art::SpriteDrawRect(const std::string &spriteName, Vector2 tileCorner, Rectangle &out,
						 float scale) const
{
	return draw_.SpriteDrawRect(sprites_, spriteName, tileCorner, out, scale);
}

void Art::DrawBuilding(BuildingType type, TeamId teamID, int tileX, int tileY,
					   BuildingFrame frame, Color tint) const
{
	draw_.DrawBuilding(sprites_, type, teamID, tileX, tileY, frame, tint);
}

void Art::DrawNode(ResourceKind kind, Vector2 center) const
{
	draw_.DrawNode(sprites_, kind, center);
}

void Art::DrawIcon(ResourceKind kind, Vector2 screenPos) const
{
	draw_.DrawIcon(sprites_, kind, screenPos);
}

void Art::DrawRallyMarker(TeamId teamID, Vector2 worldCenter) const
{
	draw_.DrawRallyMarker(sprites_, teamID, worldCenter);
}

void Art::DrawTerrain(TerrainType type, Vector2 corner) const
{
	draw_.DrawTerrain(sprites_, type, corner);
}

bool Art::HasFont() const
{
	return sprites_.HasFont();
}

const Font &Art::UiFont() const
{
	return sprites_.UiFont();
}

void Art::DrawUiText(const Art *art, const char *text, int x, int y, int size, Color color)
{
	ArtDraw::DrawUiText(art != nullptr ? &art->sprites_ : nullptr, text, x, y, size, color);
}

std::string Art::UnitSprite(UnitType type, bool moving, unsigned int id, float timeSeconds,
							int facingDir, bool attacking) const
{
	return sprites_.UnitSprite(type, moving, id, timeSeconds, facingDir, attacking);
}

std::string Art::TankBodySprite(UnitType type, int facingDir) const
{
	return sprites_.TankBodySprite(type, facingDir);
}

std::string Art::TankHeadSprite(UnitType type, int facingDir) const
{
	return sprites_.TankHeadSprite(type, facingDir);
}

bool Art::HasTankArt(UnitType type) const
{
	return sprites_.HasTankArt(type);
}

float Art::UnitDeathAnimSeconds(UnitType type) const
{
	return sprites_.UnitDeathAnimSeconds(type);
}

std::string Art::UnitDeathSprite(UnitType type, unsigned int id, float elapsedSeconds,
								 int facingDir) const
{
	return sprites_.UnitDeathSprite(type, id, elapsedSeconds, facingDir);
}

Color Art::TeamTint(TeamId teamID) const
{
	return sprites_.TeamTint(teamID);
}

void Art::SetColorBlindMode(bool enabled)
{
	sprites_.SetColorBlindMode(enabled);
}

float Art::BaseArtScale(UnitType type)
{
	return ArtSprites::BaseArtScale(type);
}

const char *Art::TerrainFile(TerrainType type)
{
	return ArtSprites::TerrainFile(type);
}

int Art::TerrainSlot(TerrainType type)
{
	return ArtSprites::TerrainSlot(type);
}

Particles &Art::ParticlesPool()
{
	return particles_;
}

const Particles &Art::ParticlesPool() const
{
	return particles_;
}
