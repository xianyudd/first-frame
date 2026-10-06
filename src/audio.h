#ifndef L4_AUDIO_H
#define L4_AUDIO_H
#include "raylib.h"

#ifndef L4_AUDIO_TEST
namespace AudioBackend {
inline Music music{};
inline Sound shot{}, hit{}, hurt{};
inline bool deviceOwned = false;
inline bool musicLoaded = false, shotLoaded = false, hitLoaded = false, hurtLoaded = false;
inline void Unload() {
    if (musicLoaded) { UnloadMusicStream(music); musicLoaded = false; }
    if (shotLoaded) { UnloadSound(shot); shotLoaded = false; }
    if (hitLoaded) { UnloadSound(hit); hitLoaded = false; }
    if (hurtLoaded) { UnloadSound(hurt); hurtLoaded = false; }
    if (deviceOwned) { CloseAudioDevice(); deviceOwned = false; }
}
inline bool Load() {
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        deviceOwned = IsAudioDeviceReady();
    }
    if (!IsAudioDeviceReady()) return false;
    music = LoadMusicStream("assets/audio/bgm.wav");
    musicLoaded = music.stream.buffer != nullptr;
    shot = LoadSound("assets/audio/shoot.wav");
    shotLoaded = shot.stream.buffer != nullptr;
    hit = LoadSound("assets/audio/hit.wav");
    hitLoaded = hit.stream.buffer != nullptr;
    hurt = LoadSound("assets/audio/hurt.wav");
    hurtLoaded = hurt.stream.buffer != nullptr;
    if (!musicLoaded || !shotLoaded || !hitLoaded || !hurtLoaded ||
        music.frameCount == 0 || shot.frameCount == 0 || hit.frameCount == 0 || hurt.frameCount == 0) {
        TraceLog(LOG_WARNING, "Game audio unavailable: check assets/audio/*.wav");
        Unload();
        return false;
    }
    music.looping = true;
    SetMusicVolume(music, 0.30f);
    SetSoundVolume(shot, 0.40f);
    SetSoundVolume(hit, 0.45f);
    SetSoundVolume(hurt, 0.45f);
    return true;
}
inline void Start() { PlayMusicStream(music); }
inline void Update() { UpdateMusicStream(music); }
inline void Pause() { PauseMusicStream(music); }
inline void Resume() { ResumeMusicStream(music); }
inline void Shot() { PlaySound(shot); }
inline void Hit() { PlaySound(hit); }
inline void Hurt() { PlaySound(hurt); }
}
#endif

namespace GameAudio {
inline bool ready = false;
inline bool paused = false;
inline bool attempted = false;
}
inline bool LoadGameAudio() {
    if (GameAudio::attempted) return GameAudio::ready;
    GameAudio::attempted = true;
    GameAudio::ready = AudioBackend::Load();
    if (GameAudio::ready) AudioBackend::Start();
    return GameAudio::ready;
}
inline void UpdateGameAudio(bool gameOver) {
    if (!GameAudio::ready) return;
    if (gameOver && !GameAudio::paused) AudioBackend::Pause();
    if (!gameOver && GameAudio::paused) AudioBackend::Resume();
    GameAudio::paused = gameOver;
    AudioBackend::Update();
}
inline void UnloadGameAudio() {
    if (GameAudio::ready) AudioBackend::Unload();
    GameAudio::ready = false;
    GameAudio::paused = false;
    GameAudio::attempted = false;
}
inline void PlayShotSound() { if (GameAudio::ready) AudioBackend::Shot(); }
inline void PlayHitSound() { if (GameAudio::ready) AudioBackend::Hit(); }
inline void PlayHurtSound() { if (GameAudio::ready) AudioBackend::Hurt(); }
#endif
