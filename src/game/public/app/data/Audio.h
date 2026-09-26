#pragma once

#include <string>

#include "raylib.h"
#include "core/Subsystem.h"

// Audio: SFX + a looped music stream from data/audio/*.wav (synthesized by
// tools/gen_audio.py). Headless-safe: Init(false) loads nothing and every
// Play is a no-op.

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
    // Load assets and start the music loop; withDevice=false loads nothing
    // and reports !IsReady. Every other method stays safe to call.
    bool Init(bool withDevice);
    void Shutdown();

    bool IsReady() const;
    void Play(SfxId id); // no-op unless ready
    // Per-sound retrigger floor; pure on (id, now), the headless-testable
    // half of Play (which feeds it GetTime).
    bool ShouldPlay(SfxId id, double now);
    void UpdateMusic();  // pump the stream; call every frame when ready

    // Volumes in 0..1 (clamped); mute zeroes the master output; persisted by
    // the settings file.
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
