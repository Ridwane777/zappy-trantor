#pragma once
// Lightweight audio layer: streamed background music + a small pool of one-shot
// sound-effect voices. Clips live in assets/audio/ as .ogg/.wav/.flac. Missing
// files are simply silent — the hooks can be wired before the audio exists.

#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class AudioManager {
public:
    // Load whatever known SFX clips exist under `dir` (assets/audio).
    void init(const std::string &dir);

    // Streamed looping/one-shot music (only one music track at a time).
    void playMusic(const std::string &name, bool loop = true, float volume = 55.f);
    void stopMusic();

    // Fire-and-forget sound effect (no-op if the clip is missing).
    void play(const std::string &name, float volume = 100.f);

    // Looping sound effect on a dedicated voice (e.g. a sustained sparkle held
    // across a phase). Call stopLoop() to end it. Only one loop voice at a time.
    void playLoop(const std::string &name, float volume = 100.f);
    void setLoopVolume(float volume);          // live-adjust the loop (e.g. a swell)
    void stopLoop();

    void setMuted(bool m) { _muted = m; if (m) stopMusic(); }
    bool muted() const { return _muted; }

