#pragma once

#include <string>

#include "raylib.h"
#include "app/data/InitMode.h"
#include "core/Subsystem.h"

enum class SfxId
{
	Select,
	Confirm,
	Attack,
	Explosion,
	Place,
	Deplete,
	Victory,
	Defeat,
	Count
};

class Audio : public Subsystem
{
public:
	Audio(); // first ShouldPlay ever is true
	// Headless loads nothing and reports !IsReady; every other method stays safe.
	bool Init(InitMode mode);
	void Shutdown();

	bool IsReady() const;
	void Play(SfxId id); // no-op unless ready
	// Per-sound retrigger floor; pure on (id, now), fed GetTime by Play.
	bool ShouldPlay(SfxId id, double now);
	void UpdateMusic();  // pump the stream; call every frame when ready

	// Volumes in 0..1 (clamped); mute zeroes the master output.
	void ApplySettings(float master, float music, float sfx, bool mute);
	float MasterVolume() const;
	float MusicVolume() const;
	float SfxVolume() const;
	bool IsMuted() const;

private:
	static const char *FileFor(SfxId id);

	Sound sounds_[static_cast<int>(SfxId::Count)] = {};
	Music music_ = {};
	bool ready_ = false;
	bool musicStarted_ = false;
	float master_ = 1.0f;
	float musicVol_ = 0.8f;
	float sfx_ = 1.0f;
	bool muted_ = false;
	double lastPlay_[static_cast<int>(SfxId::Count)] = {};
};
