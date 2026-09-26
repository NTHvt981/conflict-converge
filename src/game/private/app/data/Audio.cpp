#include "app/data/Audio.h"

namespace
{

float Clamp01(float v)
{
    if (v < 0.0f)
    {
        return 0.0f;
    }
    if (v > 1.0f)
    {
        return 1.0f;
    }
    return v;
}

const char *kAssetDir = "data/audio/";

double MinIntervalSeconds(SfxId id)
{
    switch (id)
    {
    case SfxId::Explosion:
        return 0.30;
    case SfxId::Confirm:
        return 0.20;
    case SfxId::Attack:
        return 0.10;
    case SfxId::Select:
        return 0.05;
    default:
        return 0.0;
    }
}

}

Audio::Audio()
{
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i)
    {
        lastPlay_[i] = -1.0e9;
    }
}

const char *Audio::FileFor(SfxId id)
{
    switch (id)
    {
    case SfxId::Select:
        return "select.wav";
    case SfxId::Confirm:
        return "confirm.wav";
    case SfxId::Attack:
        return "attack.wav";
    case SfxId::Explosion:
        return "explosion.wav";
    case SfxId::Place:
        return "place.wav";
    case SfxId::Deplete:
        return "deplete.wav";
    case SfxId::Victory:
        return "victory.wav";
    case SfxId::Defeat:
        return "defeat.wav";
    case SfxId::Count:
        break;
    }
    return "select.wav";
}

bool Audio::Init(bool withDevice)
{
    Shutdown();
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i)
    {
        lastPlay_[i] = -1.0e9;
    }
    if (!withDevice)
    {
        return false;
    }
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i)
    {
        const std::string path = std::string(kAssetDir) + FileFor(static_cast<SfxId>(i));
        sounds_[i] = LoadSound(path.c_str());
    }
    music_ = LoadMusicStream((std::string(kAssetDir) + "music_loop.wav").c_str());
    music_.looping = true;
    ready_ = true;
    ApplySettings(master_, musicVol_, sfx_, muted_);
    return true;
}

void Audio::Shutdown()
{
    if (!ready_)
    {
        return;
    }
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i)
    {
        UnloadSound(sounds_[i]);
        sounds_[i] = {};
    }
    UnloadMusicStream(music_);
    music_ = {};
    musicStarted_ = false;
    ready_ = false;
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i)
    {
        lastPlay_[i] = -1.0e9;
    }
}

bool Audio::IsReady() const
{
    return ready_;
}

void Audio::Play(SfxId id)
{
    if (!ready_)
    {
        return;
    }
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(SfxId::Count))
    {
        return;
    }
    if (!ShouldPlay(id, GetTime()))
    {
        return;
    }
    SetSoundVolume(sounds_[i], sfx_);
    PlaySound(sounds_[i]);
}

bool Audio::ShouldPlay(SfxId id, double now)
{
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(SfxId::Count))
    {
        return false;
    }
    if (now - lastPlay_[i] < MinIntervalSeconds(id))
    {
        return false;
    }
    lastPlay_[i] = now;
    return true;
}

void Audio::UpdateMusic()
{
    if (!ready_)
    {
        return;
    }
    if (!musicStarted_)
    {
        PlayMusicStream(music_);
        musicStarted_ = true;
    }
    UpdateMusicStream(music_);
}

void Audio::ApplySettings(float master, float music, float sfx, bool mute)
{
    master_ = Clamp01(master);
    musicVol_ = Clamp01(music);
    sfx_ = Clamp01(sfx);
    muted_ = mute;
    if (!ready_)
    {
        return;
    }
    SetMasterVolume(muted_ ? 0.0f : master_);
    SetMusicVolume(music_, musicVol_);
}

float Audio::MasterVolume() const
{
    return master_;
}

float Audio::MusicVolume() const
{
    return musicVol_;
}

float Audio::SfxVolume() const
{
    return sfx_;
}

bool Audio::IsMuted() const
{
    return muted_;
}
