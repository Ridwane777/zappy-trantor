#pragma once
// Lightweight audio layer: streamed background music + a small pool of one-shot
// sound-effect voices. Clips live in assets/audio/ as .ogg/.wav/.flac. Missing
// files are simply silent — the hooks can be wired before the audio exists.

#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <vector>

