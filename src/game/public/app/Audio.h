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
    Attack,   // strike telegraph (caller 0.12s gate + Audio floor backstop)
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
    Audio(); // last-play starts at -infinity: first ShouldPlay ever is true
    // Load assets (data/audio/*.wav) and start the music loop. With
    // withDevice=false, loads nothing and reports !IsReady; every other
    // method stays safe to call. Returns IsReady().
    bool Init(bool withDevice);
    void Shutdown();

    bool IsReady() const;
    void Play(SfxId id); // no-op unless ready
    // Per-sound retrigger floor (seconds since the same id last played).
    // Pure on (id, now): the headless-testable half of Play, which feeds it
    // GetTime(). True on first call ever (last-play starts at -infinity).
    // Victory/Defeat always return true: one-shots must never be swallowed.
    // Out-of-range ids always return false.
    bool ShouldPlay(SfxId id, double now);
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
    // Last-play timestamps (seconds, same clock as ShouldPlay's now) for
    // the retrigger floors. See MinIntervalSeconds in Audio.cpp.
    double lastPlay_[static_cast<int>(SfxId::Count)] = {};
};
