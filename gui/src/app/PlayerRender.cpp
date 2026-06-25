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
        pa.moveT = std::max(0.f, pa.moveT - dt);
        pa.turnT = std::max(0.f, pa.turnT - dt);
        int a = 0;                                    // idle
        if (frozen)                 a = 0;            // stand still for the level-up
        else if (pa.moveLerp < 1.f) a = 1;            // walk while gliding
        else if (pa.turnT > 0.f)    a = pa.turnAnim;  // turn
        if (a >= ctx.tiers[tierOf(ctx.tiers, p.level)].mesh.animCount()) a = 0;
        pa.anim = a;
        // Advance the animation clock. While walking, sync the stride to the
        // actual ground distance moved this frame so the feet don't slide
        // (one walk cycle == WALK_CYCLE_DIST world units). Idle/turn: real time.
        if (a == 1) {
            float frameDist = glm::length(pa.curWorld - beforeC);
            float cyc = ctx.tiers[tierOf(ctx.tiers, p.level)].mesh.animDuration(1);
            pa.animTime += (frameDist / WALK_CYCLE_DIST) * cyc;
        } else {
            pa.animTime += dt;
        }
        // evolution on level-up (hatch 0->1 or any +level)
        if (p.level != pa.prevLevel) {
            if (pa.prevLevel >= 0 && p.level >= 1) {
                pa.evoT = ctx.evoDur;
                pa.freezeT = ctx.evoDur;                // hold still for the level-up moment
                ctx.gameAudio.play(pa.prevLevel == 0 ? "hatch" : "levelup", 90.f);
                float wx = pa.curWorld.x, wz = pa.curWorld.y;
                for (int i = 0; i < 30; ++i) {
                    Spark s;
                    glm::vec3 d(frand(-1, 1), frand(0.2f, 1.6f), frand(-1, 1));
                    s.pos = {wx, frand(0.3f, 2.2f), wz};
                    s.vel = d * frand(1.2f, 3.2f);
                    s.maxLife = frand(0.6f, 1.3f); s.life = s.maxLife;
                    s.size = frand(0.07f, 0.17f);
                    s.phase = frand(0.f, 6.28f); s.twFreq = frand(12.f, 28.f);
                    s.color = glm::mix(glm::vec3(0.6f, 0.8f, 1.0f),
                                       glm::vec3(1.0f, 1.0f, 0.85f), frand(0.f, 1.f));
                    ctx.sparks.push_back(s);
                }
            }
            pa.prevLevel = p.level;
        }
        pa.evoT = std::max(0.f, pa.evoT - dt);
    }
}

// Runs every draw pass that depends on the player list: solid skinned
// characters, Mew (admin mount), the night moon, team-selection aura glow,
// eggs, ground level rings, evolution glow + sparkles, and the night-sky
// overlay.
static void drawPlayerPasses(PlayerRenderCtx& ctx,
                             const std::vector<const GuiPlayer*>& drawList,
                             Shader& skinShader, Shader& auraShader,
                             Shader& toon, Shader& starShader, Shader& eggShader,
                             unsigned sparkVAO,
                             const glm::mat4& view, const glm::mat4& proj,
                             const glm::vec3& camPos, const glm::vec3& sunDir,
                             const glm::vec3& lightCol, float ambScale,
                             float nightF, float elapsed, float dt) {
    glm::mat4 pBones[MAX_BONES];

    // solid character pass
    skinShader.use();
    skinShader.setMat4("uView", glm::value_ptr(view));
    skinShader.setMat4("uProj", glm::value_ptr(proj));
    skinShader.setVec3("uCamPos",          camPos.x, camPos.y, camPos.z);
    skinShader.setVec3("uDirLightDir",     sunDir.x, sunDir.y, sunDir.z);
    skinShader.setVec3("uDirLightColor",   lightCol.x, lightCol.y, lightCol.z);
    skinShader.setVec3("uPointLight0Color",0.f,0.f,0.f);
    skinShader.setVec3("uPointLight1Color",0.f,0.f,0.f);
    skinShader.setFloat("uShininess",      28.f);
    skinShader.setFloat("uAmbientStrength",0.30f * ambScale);
    skinShader.setFloat("uAlpha",          1.0f);
    skinShader.setFloat("uTime",           elapsed);
    skinShader.setBool ("uEmissive",       false);
    skinShader.setBool ("uCrystal",        false);
    skinShader.setBool ("uWave",           false);
    skinShader.setBool ("uSway",           false);
    skinShader.setBool ("uCircleClip",     false);
    skinShader.setBool ("uShoreBlend",     false);
    skinShader.setFloat("uShoreInnerR",    0.f);
    skinShader.setFloat("uShoreWidth",     1.f);
    skinShader.setBool ("uUseTexture",     true);
    skinShader.setInt  ("uTexture",        0);
    skinShader.setBool ("uUseVertexColor", false);
    skinShader.setBool ("uMultiColor",     false);
    for (const GuiPlayer* pp : drawList) {
        const GuiPlayer& p = *pp;
        if (p.level == 0) continue;              // unhatched egg — drawn below
        SkinnedMesh& m = ctx.tiers[tierOf(ctx.tiers, p.level)].mesh;
        int a = ctx.playerAnims[p.id].anim;
        m.computeBones(a, ctx.playerAnims[p.id].animTime, pBones, true, false);
        skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
        glm::mat4 M = playerModel(ctx, p);
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skinShader.setMat4("uModel",     glm::value_ptr(M));
        skinShader.setMat3("uNormalMat", glm::value_ptr(N3));
        m.draw();
    }

    // ── admin: draw Mew (the 3rd-person mount) ──
    if (ctx.adminMode && ctx.haveMew) {
        ctx.mewMesh.computeBones(0, ctx.mewAnimTime, pBones, true, false);
        skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
        // gentle float: bigger bounce when idle (hovering in place), softened
        // while moving so it doesn't fight the travel motion
        float bobAmp = ctx.mewMoving ? 0.12f : 0.32f;
        float by = ctx.mewPos.y + std::sin(elapsed * 2.2f) * bobAmp;   // float / bounce in the air
        glm::mat4 M = glm::translate(glm::mat4(1.f), glm::vec3(ctx.mewPos.x, by, ctx.mewPos.z));
        M = glm::rotate(M, ctx.mewYaw, glm::vec3(0, 1, 0));
        M = glm::scale(M, glm::vec3(ctx.mewScale));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skinShader.setMat4("uModel",     glm::value_ptr(M));
        skinShader.setMat3("uNormalMat", glm::value_ptr(N3));
        ctx.mewMesh.draw();
    }

    // ── night moon: textured, self-lit, low over the sky, fades in at night ──
    if (ctx.haveMoon && nightF > 0.01f) {
        ctx.moonMesh.computeBones(0, 0.f, pBones, true, false);
        skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
        // mesh radius ~50 in local space; sky moon ≈ radius 250 at 1600 units
        glm::vec3 mp = camPos + ctx.moonDir * 1600.f;
        glm::mat4 M  = glm::translate(glm::mat4(1.f), mp);
        M = glm::scale(M, glm::vec3(5.0f));
        glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
        skinShader.setMat4 ("uModel",           glm::value_ptr(M));
        skinShader.setMat3 ("uNormalMat",       glm::value_ptr(N3));
        skinShader.setBool ("uUseTexture",      true);
        skinShader.setFloat("uAmbientStrength", 1.5f);     // self-lit, bright
        skinShader.setVec3 ("uDirLightColor",   0.f, 0.f, 0.f);
        skinShader.setFloat("uAlpha",           nightF);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        ctx.moonMesh.draw();
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        skinShader.setFloat("uAlpha", 1.0f);               // restore
    }

    // ── team-selection glow: selected team's characters glow team color ──
    const std::string& selTeam = ctx.hud.selectedTeam();
    if (!selTeam.empty()) {
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);             // additive
        auraShader.use();
        auraShader.setMat4("uView", glm::value_ptr(view));
        auraShader.setMat4("uProj", glm::value_ptr(proj));
        int ti = 0;
        { const GuiState& gs = ctx.netClient.state();
          for (int k = 0; k < (int)gs.teams.size(); ++k)
              if (gs.teams[k] == selTeam) ti = k; }
        glm::vec3 tc = HudOverlay::teamColor3(ti);
        float gp = 0.55f + 0.45f * std::sin(elapsed * 4.f);  // steady pulse
        for (const GuiPlayer* pp : drawList) {
            const GuiPlayer& p = *pp;
            if (p.team != selTeam || p.level < 1) continue;
            SkinnedMesh& m = ctx.tiers[tierOf(ctx.tiers, p.level)].mesh;
            m.computeBones(ctx.playerAnims[p.id].anim, ctx.playerAnims[p.id].animTime,
                           pBones, true, false);
            auraShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
            auraShader.setVec3 ("uColor", tc.r, tc.g, tc.b);
            auraShader.setFloat("uAlpha", 0.30f + 0.35f * gp);
            glm::mat4 M = glm::scale(playerModel(ctx, p), glm::vec3(1.06f));
            auraShader.setMat4("uModel", glm::value_ptr(M));
            m.draw();
        }
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }

    // eggs: level-0 players are unhatched eggs (textured)
    if (ctx.eggMeshTex) {
        float r  = ctx.eggMeshTex->boundingRadius();
        float es = (r > 1e-4f) ? 0.15f / r : 0.15f;   // small, fits lvl-1 char
        glm::vec3 ec = ctx.eggMeshTex->centroid();
        eggShader.use();
        eggShader.setMat4 ("uView", glm::value_ptr(view));
        eggShader.setMat4 ("uProj", glm::value_ptr(proj));
        eggShader.setInt  ("uTex", 0);
        eggShader.setVec3 ("uDirLightDir",   sunDir.x, sunDir.y, sunDir.z);
        eggShader.setVec3 ("uDirLightColor", lightCol.x, lightCol.y, lightCol.z);
        eggShader.setVec3 ("uPointPos",   0.f, 10.f, 0.f);
        eggShader.setVec3 ("uPointColor", 0.f, 0.f, 0.f);
        eggShader.setFloat("uAmbient", 0.45f * ambScale);
        eggShader.setFloat("uAlpha",   1.f);
        eggShader.setFloat("uBright",  1.f);
        glDisable(GL_CULL_FACE);
        for (const GuiPlayer* pp : drawList) {
            const GuiPlayer& p = *pp;
            if (p.level != 0) continue;
            glm::vec2 ew = ctx.playerAnims[p.id].curWorld;
            float wx  = ew.x;
            float wz  = ew.y;
            float wob = std::sin(elapsed * 3.f + p.id) * 0.06f;   // gentle wobble
            glm::mat4 M = glm::translate(glm::mat4(1.f), {wx, es * r, wz});
            M = glm::rotate(M, wob, {0.f, 0.f, 1.f});
            M = glm::scale(M, glm::vec3(es));
            M = glm::translate(M, -ec);
            glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
            eggShader.setMat4("uModel",     glm::value_ptr(M));
            eggShader.setMat3("uNormalMat", glm::value_ptr(N3));
            ctx.eggMeshTex->draw();
        }
        glEnable(GL_CULL_FACE);
    }

    // level ring on the ground: purple for odd levels, white for even
    static Mesh levelRing = buildRingMesh();      // flat annulus, radius 1
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);            // additive glow
    toon.use();
    toon.setBool ("uEmissive", true);
    toon.setBool ("uCircleClip", false);
    toon.setBool ("uShoreBlend", false);
    float ringPulse = 0.70f + 0.30f * std::sin(elapsed * 3.f);
