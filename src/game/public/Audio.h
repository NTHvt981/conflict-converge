#pragma once

#include <string>

#include "raylib.h" // Sound, Music (device-independent types)

// M11: audio. SFX + music loop live as .wav assets under data/audio/,
// synthesized by tools/gen_audio.py (rfxgen is interactive-only, so the
// script produces the equivalent rfxgen-style blips reproducibly).
// Headless-safe: Init(false) loads nothing and every Play is a no-op, so
// the test binary never needs an audio device. The game calls Init(true)
// after InitAudioDevice and guards playback with IsReady().

enum class SfxId
{
    Select,   // unit selected
    Confirm,  // order acknowledged / unit produced
    Attack,   // strike telegraph (rate-limited by the caller)
    Explosion, // unit destroyed
    Place,    // building placed
    Deplete,  // resource node exhausted
    Victory,
    Defeat,
    Count
};

class Audio
{
public:
    // Load assets (data/audio/*.wav) and start the music loop. With
    // withDevice=false, loads nothing and reports !IsReady; every other
    // method stays safe to call. Returns IsReady().
    bool Init(bool withDevice);
    void Shutdown();

    bool IsReady() const;
    void Play(SfxId id); // no-op unless ready
    void UpdateMusic();  // pump the stream; call every frame when ready

    // Volumes in 0..1 (clamped); mute zeroes the master output.
    // Persisted by the M14 settings file (data/settings.cfg).
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
};
