#include "app/data/Art.h"

void Art::Shutdown()
{
	for (int t = 0; t < static_cast<int>(UnitType::Count); ++t)
	{
		for (int team = 0; team < 2; ++team)
		{
			for (int f = 0; f < 2; ++f)
			{
				if (units_[t][team][f].id != 0)
				{
					UnloadTexture(units_[t][team][f]);
					units_[t][team][f] = {};
				}
			}
		}
	}
	for (int t = 0; t < static_cast<int>(BuildingType::Count); ++t)
	{
		for (int f = 0; f < static_cast<int>(BuildingFrame::Count); ++f)
		{
			BuildingFrameArt &frame = buildingArt_[t].frames[f];
			for (int team = 0; team < 2; ++team)
			{
				if (frame.flat[team].id != 0)
				{
					UnloadTexture(frame.flat[team]);
					frame.flat[team] = {};
				}
			}
			if (frame.base.id != 0)
			{
				UnloadTexture(frame.base);
				frame.base = {};
			}
			if (frame.mask.id != 0)
			{
				UnloadTexture(frame.mask);
				frame.mask = {};
			}
			buildingArt_[t].hasFrame[f] = false;
		}
	}
	for (int k = 0; k < 2; ++k)
	{
		if (nodes_[k].id != 0)
		{
			UnloadTexture(nodes_[k]);
			nodes_[k] = {};
		}
		if (icons_[k].id != 0)
		{
			UnloadTexture(icons_[k]);
			icons_[k] = {};
		}
	}
	if (rallyBase_.id != 0)
	{
		UnloadTexture(rallyBase_);
		rallyBase_ = {};
	}
	if (rallyMask_.id != 0)
	{
		UnloadTexture(rallyMask_);
		rallyMask_ = {};
	}
	for (int s = 0; s < 4; ++s)
	{
		if (terrain_[s].id != 0)
		{
			UnloadTexture(terrain_[s]);
			terrain_[s] = {};
		}
	}
	if (fontReady_)
	{
		UnloadFont(uiFont_);
		uiFont_ = {};
		fontReady_ = false;
	}
	for (const auto &entry : atlas_)
	{
		UnloadTexture(entry.second);
	}
	atlas_.clear();
	sheet_ = SpriteSheetData{};
	atlasReady_ = false;
	sheetLoaded_ = false;
	particles_.Clear();
	ready_ = false;
	fallback_ = true;
}

bool Art::IsReady() const
{
	return ready_;
}

bool Art::UseRectangles() const
{
	return fallback_;
}

bool Art::UseAtlas() const
{
	return atlasReady_;
}

Particles &Art::ParticlesPool()
{
	return particles_;
}

const Particles &Art::ParticlesPool() const
{
	return particles_;
}
