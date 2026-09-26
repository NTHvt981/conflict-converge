// Unit tests for audio (headless guard, volume clamps, settings defaults).
// Nothing here touches an audio device: Audio::Init(false) must load nothing
// and every Play must no-op, so the suite runs on machines without sound.

#include "test_harness.h"

#include "app/data/Audio.h"
#include "app/match/Menu.h" // MenuSettings volume defaults

void RunAudioTests()
{
    // --- uninitialized audio is inert ---
    {
        Audio audio;
        CC_CHECK(!audio.IsReady());
        for (int i = 0; i < static_cast<int>(SfxId::Count); ++i)
        {
            audio.Play(static_cast<SfxId>(i)); // must not crash
        }
        audio.UpdateMusic(); // must not crash
        audio.Shutdown();    // must not crash
        CC_CHECK(!audio.IsReady());
    }

    // --- headless init loads nothing but still records settings ---
    {
        Audio audio;
        CC_CHECK(!audio.Init(false));
        CC_CHECK(!audio.IsReady());
        audio.Play(SfxId::Explosion);
        audio.UpdateMusic();
        audio.ApplySettings(0.5f, 0.25f, 0.75f, false);
        CC_CHECK(audio.MasterVolume() == 0.5f);
        CC_CHECK(audio.MusicVolume() == 0.25f);
        CC_CHECK(audio.SfxVolume() == 0.75f);
        CC_CHECK(!audio.IsMuted());
        audio.Shutdown();
    }

    // --- volumes clamp to 0..1, mute latches ---
    {
        Audio audio;
        audio.ApplySettings(2.0f, -1.0f, 99.0f, true);
        CC_CHECK(audio.MasterVolume() == 1.0f);
        CC_CHECK(audio.MusicVolume() == 0.0f);
        CC_CHECK(audio.SfxVolume() == 1.0f);
        CC_CHECK(audio.IsMuted());
        audio.ApplySettings(0.3f, 0.3f, 0.3f, false);
        CC_CHECK(!audio.IsMuted());
    }

    // --- out-of-range SFX ids never crash, even uninitialized ---
    {
        Audio audio;
        audio.Play(static_cast<SfxId>(-1));
        audio.Play(static_cast<SfxId>(static_cast<int>(SfxId::Count)));
        audio.Play(SfxId::Count);
    }

    // --- menu settings carry sane audio defaults ---
    {
        MenuSettings settings;
        CC_CHECK(settings.masterVolume == 1.0f);
        CC_CHECK(settings.musicVolume == 0.8f);
        CC_CHECK(settings.sfxVolume == 1.0f);
        CC_CHECK(!settings.mute);
    }

    // --- per-sound rate limiting: combat retrigger storms collapse ---
    // (Game::Update plays Explosion on every death-frame and Attack every
    // 0.12s while anything winds up; without floors that is a 60Hz roar.)
    {
        Audio audio;
        CC_CHECK(audio.ShouldPlay(SfxId::Explosion, 100.0));
        CC_CHECK(!audio.ShouldPlay(SfxId::Explosion, 100.1));
        CC_CHECK(!audio.ShouldPlay(SfxId::Explosion, 100.29));
        CC_CHECK(audio.ShouldPlay(SfxId::Explosion, 100.31));
        CC_CHECK(audio.ShouldPlay(SfxId::Confirm, 50.0));
        CC_CHECK(!audio.ShouldPlay(SfxId::Confirm, 50.1));
        CC_CHECK(audio.ShouldPlay(SfxId::Confirm, 50.25));
        // Victory/Defeat are never throttled (one-shots must always land).
        CC_CHECK(audio.ShouldPlay(SfxId::Victory, 0.0));
        CC_CHECK(audio.ShouldPlay(SfxId::Victory, 0.0));
        CC_CHECK(audio.ShouldPlay(SfxId::Defeat, 0.0));
        // Floors are per-sound: Attack plays even right after an Explosion.
        CC_CHECK(audio.ShouldPlay(SfxId::Attack, 100.15));
        // Out-of-range ids never play, never crash.
        CC_CHECK(!audio.ShouldPlay(static_cast<SfxId>(-1), 100.0));
        CC_CHECK(!audio.ShouldPlay(SfxId::Count, 100.0));
    }
}
