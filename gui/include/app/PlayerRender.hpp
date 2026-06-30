#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "render/SkinnedMesh.hpp"

class Shader;
class GuiClient;
class HudOverlay;
class Constellation;
class AudioManager;
class TexturedMesh;
struct GuiPlayer;
struct MapData;
struct Spark;

// One skinned mesh per 2-level band (1-2,3-4,5-6,7-8), shared by every player
// in that band.
struct PlayerTier { SkinnedMesh mesh; float scale = 0.05f; bool loaded = false; };

// Tier index for a level (0=lvl1-2 ... 3=lvl7-8); falls back to a loaded tier.
int tierOf(const PlayerTier tiers[4], int level);

// Per-player animation/evolution state, keyed by player id. Persists across
// frames so movement/turns/level-ups can be detected from discrete server
// updates and smoothed (walk glide, turn blend, evolution flash timing).
struct PlayerAnim {
    float animTime = 0.f;
    int   px = -9999, py = -9999, po = -1;
    float moveT = 0.f, turnT = 0.f;
    int   turnAnim = 3;
    int   prevLevel = -1;      // for level-up (evolution) detection
    float evoT = 0.f;          // remaining evolution-effect time
    float freezeT = 0.f;       // hold still during the level-up (matches sfx length)
    glm::vec2 curWorld{0.f}, prevWorld{0.f};  // smooth walk interpolation
    float moveLerp = 1.f;
    bool  posInit = false;
    int   anim = 0;            // current animation index
    int   prevAnim = 0;        // animation we're blending away from
    float prevAnimTime = 0.f;
    float blendT = 0.f;        // counts down from ANIM_BLEND_DUR to 0
};

// Shared state for the live/demo player pass, pulled out of GuiApp's run()
// loop. Everything here is owned by GuiApp and persists across frames; this
// struct just bundles references so the render passes below can share them
// without a 25-parameter function signature.
struct PlayerRenderCtx {
    PlayerTier (&tiers)[4];
    std::unordered_map<int, PlayerAnim>& playerAnims;
    std::vector<Spark>& sparks;

    bool testMode;
    bool netConnected;
    const std::vector<GuiPlayer>& testPlayers;
    GuiClient& netClient;
    const MapData& map;
    float evoDur;
    AudioManager& gameAudio;

    bool adminMode, haveMew;
    SkinnedMesh& mewMesh;
    float mewAnimTime; bool mewMoving;
    glm::vec3 mewPos; float mewYaw; float mewScale;

    bool haveCharz;
    SkinnedMesh& charzMesh;
    int   charzAnimIdx;
    float charzAnimTime;
    bool  charzStripRoot;   // strip baked locomotion drift (walk/run loops only)
    float charzBobAmp;
    glm::vec3 charzPos; float charzYaw; float charzScale;

    bool haveMoon;
    SkinnedMesh& moonMesh;
    glm::vec3 moonDir;

    TexturedMesh* eggMeshTex;

    HudOverlay& hud;
    Constellation& constellation;
};

// Builds the unified draw list (demo or live players), advances every
// player's walk/turn/evolution state, then runs every draw pass that depends
// on it: solid skinned characters, Mew (admin mount), the night moon,
// team-selection aura glow, eggs, ground level rings, evolution glow +
// sparkles, and the night-sky/constellation overlay.
void updateAndDrawPlayers(PlayerRenderCtx& ctx,
                          Shader& skinShader, Shader& auraShader,
                          Shader& toon, Shader& starShader, Shader& eggShader,
                          unsigned sparkVAO,
                          const glm::mat4& view, const glm::mat4& proj,
                          const glm::vec3& camPos, const glm::vec3& sunDir,
                          const glm::vec3& lightCol, float ambScale,
                          float nightF, float elapsed, float dt);
