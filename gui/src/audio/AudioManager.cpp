#include "audio/AudioManager.hpp"
#include "AssetCache.hpp"
#include <fstream>
#include <iostream>

// Known SFX clip names — generate these into assets/audio/ (any of .ogg/.wav/.flac):
//   intro SFX: whoosh, shimmer_hold, burst, reveal, twinkle
//   gameplay : click, levelup, hatch
// Known music names: intro, menu_music, game_music
static const char *KNOWN_SFX[] = {
    "sparkle", "bounce", "shimmer_hold", "burst", "reveal", "lensflare", "twinkle",
    "whoosh", "click", "levelup", "hatch", "incant", "death", "pickup",
    "mew_cry", "mew_float", "mew_fly", "mew_meow_a", "mew_meow_b", "mew_meow_c",
};

bool AudioManager::_resolve(const std::string &name, std::string &outPath) const
{
    // Pick the path to load from: a local file if one exists (any container),
    // otherwise the canonical CDN .wav (streamed in memory by assetBytes()).
    for (const char *ext : { ".ogg", ".wav", ".flac" }) {
        std::string p = _dir + "/" + name + ext;
        std::ifstream f(p);
        if (f.good()) { outPath = p; return true; }
