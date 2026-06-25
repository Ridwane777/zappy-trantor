#include <GL/glew.h>
#include "app/PlayerRender.hpp"
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "render/TexturedMesh.hpp"
#include "net/GuiClient.hpp"
#include "world/World.hpp"
#include "hud/HudOverlay.hpp"
#include "app/Constellation.hpp"
#include "app/Particle.hpp"
#include "audio/AudioManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

int tierOf(const PlayerTier tiers[4], int level) {
    int t = std::min(std::max((level - 1) / 2, 0), 3);
    for (int k = 0; k < 4 && !tiers[t].loaded; ++k) t = (t + 1) % 4;
    return t;
}

// Map a server orientation (1=N 2=E 3=S 4=W) to a model yaw in degrees.
static float orientationYaw(int o) {
    switch (o) { case 1: return 180.f; case 2: return 90.f;
                 case 3: return 0.f;   case 4: return 270.f; }
    return 0.f;
}

static float frand(float a, float b) {
    return a + (b - a) * ((float)std::rand() / (float)RAND_MAX);
}

// raw cell->world mapping (demo: island tiles; live: spread on land)
static glm::vec2 worldRaw(PlayerRenderCtx& ctx, const GuiPlayer& p) {
    if (ctx.testMode)
        return { p.x * TILE + TILE * 0.5f, p.y * TILE + TILE * 0.5f };
    const GuiState& gs = ctx.netClient.state();
    int W = gs.sizeKnown ? gs.width : ctx.map.w;
    int H = gs.sizeKnown ? gs.height : ctx.map.h;
    const float insetT = 7.f;
    float landX = std::max(1.f, ctx.map.w - 2.f * insetT);
    float landY = std::max(1.f, ctx.map.h - 2.f * insetT);
    return { (insetT + (p.x + 0.5f) * landX / W) * TILE,
             (insetT + (p.y + 0.5f) * landY / H) * TILE };
}

// smaller overall, and grows with level (lvl1 ~0.44x -> lvl8 ~0.80x)
static float scaleOf(PlayerRenderCtx& ctx, int level) {
    int L = level < 1 ? 1 : (level > 8 ? 8 : level);
    return ctx.tiers[tierOf(ctx.tiers, level)].scale * (0.44f + 0.051f * (L - 1));
}

static glm::mat4 playerModel(PlayerRenderCtx& ctx, const GuiPlayer& p) {
    glm::vec2 w = ctx.playerAnims[p.id].curWorld;     // smooth-interpolated
    glm::mat4 M = glm::translate(glm::mat4(1.f), {w.x, 0.f, w.y});
    M = glm::rotate(M, glm::radians(orientationYaw(p.orientation)), {0.f,1.f,0.f});
    M = glm::scale(M, glm::vec3(scaleOf(ctx, p.level)));
    return M;
}

// Advances every player's smooth walk interpolation, turn blend, and
// evolution/level-up timers; spawns the level-up sparks burst.
static void updatePlayerStates(PlayerRenderCtx& ctx,
                               const std::vector<const GuiPlayer*>& drawList,
                               float dt) {
    const float MOVE_DUR = 0.50f;                     // seconds to glide a tile (≈2 tiles/s)
    // World distance covered by one full walk cycle (foot-slide tuning):
    // smaller = legs cycle faster / more steps per tile. Override via env.
    static const float WALK_CYCLE_DIST = std::getenv("ZAPPY_STRIDE")
        ? std::max(0.2f, (float)std::atof(std::getenv("ZAPPY_STRIDE"))) : TILE;
    for (const GuiPlayer* pp : drawList) {
        const GuiPlayer& p = *pp;
        PlayerAnim& pa = ctx.playerAnims[p.id];
        glm::vec2 target = worldRaw(ctx, p);
        if (!pa.posInit) { pa.curWorld = pa.prevWorld = target; pa.posInit = true; }
        pa.freezeT = std::max(0.f, pa.freezeT - dt);
        bool frozen = pa.freezeT > 0.f;               // hold still during a level-up
        if (!frozen && (p.x != pa.px || p.y != pa.py)) {   // moved (skipped while frozen)
            const GuiState& gs2 = ctx.netClient.state();
            int W = gs2.sizeKnown ? gs2.width : ctx.map.w;
            int H = gs2.sizeKnown ? gs2.height : ctx.map.h;
            // The map is a torus: stepping off an edge wraps the grid coord
            // to the far side. Don't glide across the whole island for that —
            // snap instantly, like the player really did at the edge.
            bool wrap = std::abs(p.x - pa.px) > W / 2 ||
                        std::abs(p.y - pa.py) > H / 2;
            if (wrap) {
                pa.prevWorld = target; pa.curWorld = target; pa.moveLerp = 1.f;
            } else {                                  // normal step -> glide there
                pa.prevWorld = pa.curWorld; pa.moveLerp = 0.f; pa.moveT = 0.5f;
            }
            pa.px = p.x; pa.py = p.y;
        }
        if (!frozen && p.orientation != pa.po) {      // turned (skipped while frozen)
            if (pa.po != -1) {
                int diff = (p.orientation - pa.po + 4) % 4;
                pa.turnAnim = (diff == 3) ? 3 : 4;    // CCW=left else right
                pa.turnT = 0.4f;
            }
            pa.po = p.orientation;
        }
        glm::vec2 beforeC = pa.curWorld;              // for ground-distance sync
        if (!frozen) {
            pa.moveLerp = std::min(1.f, pa.moveLerp + dt / MOVE_DUR);
            float sm = pa.moveLerp * pa.moveLerp * (3.f - 2.f * pa.moveLerp);  // smoothstep
            pa.curWorld = glm::mix(pa.prevWorld, target, sm);
        }
