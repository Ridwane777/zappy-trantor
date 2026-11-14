#include <GL/glew.h>
#include <SFML/Graphics/Image.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "app/ScenePasses.hpp"
#include "AssetCache.hpp"
#include "app/CharacterRender.hpp"
#include "app/Constellation.hpp"
#include "app/Islands.hpp"
#include "app/TileDebug.hpp"
#include "app/TreeMotes.hpp"
#include "app/WorldRender.hpp"
#include "hud/HudOverlay.hpp"
#include "net/INetworkClient.hpp"
#include "render/Shader.hpp"
#include "world/World.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

// ---------------------------------------------------------------- SkyPass

SkyPass::SkyPass(Shader &sky) : _sky(sky)
{
    float quadV[] = { -1,-1, 1,-1, 1,1, -1,1 };
    unsigned int quadI[] = { 0,1,2, 0,2,3 };
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadV), quadV, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadI), quadI, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

SkyPass::~SkyPass()
{
    glDeleteBuffers(1, &_ebo);
    glDeleteBuffers(1, &_vbo);
    glDeleteVertexArrays(1, &_vao);
}

void SkyPass::draw(const FrameContext &f)
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glm::mat4 invVP = glm::inverse(f.proj * f.view);
    _sky.use();
    _sky.setVec3("uSkyTop",      f.skyTop.x, f.skyTop.y, f.skyTop.z);
    _sky.setVec3("uSkyHorizon",  f.skyHor.x, f.skyHor.y, f.skyHor.z);
    _sky.setMat4("uInvViewProj", glm::value_ptr(invVP));
    _sky.setVec3("uCamPos",      f.camPos.x, f.camPos.y, f.camPos.z);
    _sky.setVec3("uSunDir",      f.skySunDir.x, f.skySunDir.y, f.skySunDir.z);
    _sky.setVec3("uMoonDir",     f.moonDir.x, f.moonDir.y, f.moonDir.z);
    _sky.setFloat("uSunStrength",  1.f - f.nightF);
    _sky.setFloat("uMoonStrength", f.nightF);
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

// -------------------------------------------------------------- WorldPass

WorldPass::WorldPass(Shader &toon, Shader &outline, Shader &waterMap, MeshCache &meshes,
                     WorldRefs world, Mesh &land, Mesh &ocean, Mesh &water,
                     const float &treeGlow)
    : _toon(toon), _outline(outline), _waterMap(waterMap), _meshes(meshes),
      _world(world), _land(land), _ocean(ocean), _water(water), _treeGlow(treeGlow)
{
    sf::Image wi;
    const std::string& wb = zappy::assetBytes("assets/screen/water.png");
    if (!wb.empty() && wi.loadFromMemory(wb.data(), wb.size())) {
        glGenTextures(1, &_waterTex);
        glBindTexture(GL_TEXTURE_2D, _waterTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wi.getSize().x, wi.getSize().y,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, wi.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        std::cerr << "Water map texture load failed - shore water falls back to flat colour\n";
    }
}

WorldPass::~WorldPass()
{
    if (_waterTex) glDeleteTextures(1, &_waterTex);
}

void WorldPass::draw(const FrameContext &f)
{
    Shader &toon = _toon;
    toon.use();
    toon.setMat4("uView",              glm::value_ptr(f.view));
    toon.setMat4("uProj",              glm::value_ptr(f.proj));
    toon.setVec3("uCamPos",            f.camPos.x, f.camPos.y, f.camPos.z);
    toon.setVec3("uDirLightDir",       f.sunDir.x, f.sunDir.y, f.sunDir.z);
    toon.setVec3("uDirLightColor",     f.lightCol.x, f.lightCol.y, f.lightCol.z);
    toon.setVec3("uPointLight0Pos",    0.f, 10.f, 0.f);
    toon.setVec3("uPointLight0Color",  0.f, 0.f, 0.f);
    toon.setVec3("uPointLight1Pos",    0.f, 10.f, 0.f);
    toon.setVec3("uPointLight1Color",  0.f, 0.f, 0.f);
    toon.setFloat("uShininess",        32.f);
    toon.setFloat("uAmbientStrength",  0.10f * f.ambScale);
    toon.setFloat("uAlpha",       1.0f);
    toon.setFloat("uTime",        f.elapsed);
    toon.setBool ("uWave",           false);
    toon.setBool ("uSway",           false);
    toon.setBool ("uUseVertexColor", false);
    toon.setBool ("uUseTexture",    false);
    toon.setBool ("uMultiColor",    false);
    toon.setBool ("uCrystal",     false);
    toon.setBool ("uCircleClip",  false);
    toon.setBool ("uShoreBlend",  false);
    toon.setFloat("uShoreInnerR", 0.f);
    toon.setFloat("uShoreWidth",  1.f);

    glm::vec2 sunXZ = glm::normalize(glm::vec2(f.sunDir.x, f.sunDir.z) + 1e-6f);
    drawGround(toon, _waterMap, _waterTex, f.view, f.proj, f.elapsed, sunXZ,
               _land, _ocean, _water, _world.map, _world.cx, _world.cz, 1);
    drawProps(toon, _outline, _meshes, _world.map, f.view, f.proj, f.elapsed,
              f.ambScale, _treeGlow, _world.cx, _world.cz, 1);
}

// ----------------------------------------------------------- ResourcePass

ResourcePass::ResourcePass(Shader &egg, WorldRefs world) : _egg(egg), _world(world)
{
    const char* foodName[3] = { "food1", "food2", "food3" };
    for (int i = 0; i < 3; ++i) {
        try {
            _foodMesh[i] = std::make_unique<TexturedMesh>(TexturedMesh::load(
                std::string("assets/models/") + foodName[i] + ".obj",
                std::string("assets/models/") + foodName[i] + ".png"));
        } catch (const std::exception& e) {
            std::cerr << "Food " << foodName[i] << " load: " << e.what() << "\n";
        }
    }
    const char* gemName[7] = { nullptr, "linemate", "deraumere", "sibur",
                               "mendiane", "phiras", "thystame" };
    for (int i = 1; i < 7; ++i) {
        try {
            _gemTex[i] = std::make_unique<TexturedMesh>(TexturedMesh::load(
                std::string("assets/models/gem_") + gemName[i] + "_lod.obj",
                std::string("assets/models/gem_") + gemName[i] + ".png"));
        } catch (const std::exception& e) {
            std::cerr << "Gem " << gemName[i] << " load: " << e.what() << "\n";
        }
    }
}

void ResourcePass::draw(const FrameContext &f)
{
    drawResources(_egg, _gemTex, _world.map, f.sunDir, f.lightCol, f.ambScale,
                  f.view, f.proj, f.elapsed, _world.cx, _world.cz, 1);
    drawFood(_egg, _foodMesh, _world.map, f.sunDir, f.lightCol, f.ambScale,
             f.view, f.proj, f.elapsed, _world.cx, _world.cz, 1);
}

// ------------------------------------------------------------- PlayerPass

static Mesh buildBeamMesh()
{
    const int N = 32;
    const float rBot = 0.65f, rTop = 0.04f, H = 16.f;
    std::vector<float> v;
    std::vector<unsigned int> idx;
    v.reserve(N * 4 * 6);
    idx.reserve(N * 6);
    for (int i = 0; i < N; ++i) {
        float a0 = (float)i       / N * 6.28318f;
        float a1 = (float)(i + 1) / N * 6.28318f;
        auto push = [&](float r, float y, float a) {
            float nx = std::cos(a), nz = std::sin(a);
            float slope = (rBot - rTop) / H;
            float nl = std::sqrt(nx*nx + slope*slope + nz*nz);
            v.push_back(r * nx); v.push_back(y); v.push_back(r * nz);
            v.push_back(nx/nl);  v.push_back(slope/nl); v.push_back(nz/nl);
        };
        unsigned b = (unsigned)v.size() / 6;
        push(rBot, 0.f, a0); push(rBot, 0.f, a1);
        push(rTop, H,   a1); push(rTop, H,   a0);
        idx.push_back(b);   idx.push_back(b+1); idx.push_back(b+2);
        idx.push_back(b);   idx.push_back(b+2); idx.push_back(b+3);
    }
    return Mesh(v, idx);
}

PlayerPass::PlayerPass(const PlayerRenderCtx &ctx, Shaders shaders, unsigned sparkVAO,
                       std::vector<Character> &characters, const HudOverlay &hud)
    : _ctx(ctx), _sh(shaders), _sparkVAO(sparkVAO), _characters(characters),
      _hud(hud), _beamMesh(buildBeamMesh())
{
}

void PlayerPass::draw(const FrameContext &f)
{
    _ctx.moonDir = f.moonDir;
    updateAndDrawPlayers(_ctx, _sh.skin, _sh.aura, _sh.toon, _sh.star, _sh.egg,
                         _sparkVAO, f.view, f.proj, f.camPos, f.sunDir, f.lightCol,
                         f.ambScale, f.nightF, f.elapsed, f.dt);
    drawCharacters(_characters, _sh.skin, _sh.aura, f.view, f.proj,
                   f.camPos, f.sunDir, f.lightCol, f.ambScale, f.elapsed, f.dt);
    drawSelectionBeam(f);
}

void PlayerPass::drawSelectionBeam(const FrameContext &f)
{
    if (_hud.selectedPlayer() < 0 || !f.net)
        return;
    auto bpait = _ctx.playerAnims.find(_hud.selectedPlayer());
    if (bpait == _ctx.playerAnims.end() || !bpait->second.posInit)
        return;

    Shader &toon = _sh.toon;
    glm::vec2 bw = bpait->second.curWorld;
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    toon.use();
    toon.setBool ("uEmissive",   true);
    toon.setBool ("uCircleClip", false);
    toon.setBool ("uShoreBlend", false);
    toon.setBool ("uWave",       false);
    toon.setBool ("uSway",       false);
    toon.setFloat("uAlpha",      1.f);
    glm::mat4 Bm = glm::translate(glm::mat4(1.f), {bw.x, 0.f, bw.y});
    glm::mat3 Bn = glm::mat3(1.f);
    toon.setMat3("uNormalMat", glm::value_ptr(Bn));

    float pulse = 0.14f + 0.05f * std::sin(f.elapsed * 2.2f);
    toon.setMat4("uModel", glm::value_ptr(Bm));
    toon.setVec3("uColor", pulse * 1.1f, pulse * 0.9f, pulse * 0.4f);
    _beamMesh.draw();

    glm::mat4 Bc = glm::scale(Bm, glm::vec3(0.28f, 1.f, 0.28f));
    toon.setMat4("uModel", glm::value_ptr(Bc));
    float pulse2 = 0.38f + 0.12f * std::sin(f.elapsed * 3.1f);
    toon.setVec3("uColor", pulse2, pulse2 * 0.97f, pulse2 * 0.80f);
    _beamMesh.draw();
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    toon.setBool("uEmissive", false);
}

// ---------------------------------------------------------- small passes

TreeMotesPass::TreeMotesPass(Shader &star, unsigned sparkVAO, const TreePulse &pulse,
                             const glm::vec3 &treePos)
    : _star(star), _sparkVAO(sparkVAO), _pulse(pulse), _treePos(treePos)
{
}

void TreeMotesPass::draw(const FrameContext &f)
{
    drawTreeMotes(_motes, _pulse.charging, _pulse.chargeT, _pulse.justReleased, _treePos,
                  _star, _sparkVAO, f.view, f.proj, f.elapsed, f.dt);
}

MewEnergyPass::MewEnergyPass(Constellation &constellation, Shader &star, unsigned sparkVAO,
                             const bool &adminMode, const glm::vec3 &mewPos)
    : _constellation(constellation), _star(star), _sparkVAO(sparkVAO),
      _adminMode(adminMode), _mewPos(mewPos)
{
}

void MewEnergyPass::draw(const FrameContext &f)
{
    _constellation.updateMewEnergy(_adminMode, f.dt, _mewPos, f.elapsed);
    _constellation.renderMewEnergy(_star, _sparkVAO, f.view, f.proj, f.elapsed);
}

IncantationPass::IncantationPass(Shader &incant, const std::vector<IncantationEffect> &fx)
    : _incant(incant), _fx(fx)
{
}

void IncantationPass::draw(const FrameContext &f)
{
    drawIncantations(_fx, _incant, f.view, f.proj, f.camPos, f.elapsed);
}

CloudPass::CloudPass(Shader &toon, MeshCache &meshes, const MapData &map)
    : _toon(toon), _meshes(meshes), _map(map)
{
}

void CloudPass::draw(const FrameContext &f)
{
    drawClouds(_toon, _meshes, _map, f.elapsed);
}

TileGridPass::TileGridPass(WorldRefs world) : _world(world)
{
}

void TileGridPass::draw(const FrameContext &f)
{
    if (!f.showTileGrid)
        return;
    static const GuiState emptyGs{};
    const GuiState &gs = f.net ? f.net->state() : emptyGs;
    drawTileGridGL(f.view, f.proj, _world.map, gs, _world.cx, _world.cz);
}

// ----------------------------------------------------------- WildlifePass

WildlifePass::WildlifePass(Shader &skin, WorldRefs world) : _skin(skin), _world(world)
{
    const float TILE_F = TILE;
    const float cx = world.cx, cz = world.cz;
    auto wfrand = [](float lo, float hi) {
        return lo + (hi - lo) * (std::rand() / (float)RAND_MAX);
    };
    auto spawnXZ = [&](float rad) {
        float a = wfrand(0.f, 6.2831853f);
        float r = rad * std::sqrt(wfrand(0.f, 1.f));
        return glm::vec2(cx + r * std::cos(a), cz + r * std::sin(a));
    };
    const float spawnR = world.map.w * TILE_F * 0.455f;

    try {
        _ptero.mesh = SkinnedMesh::load("assets/wildlife/ptero/scene.gltf");
        _ptero.mesh.loadTexture("assets/wildlife/ptero/textures/material0_baseColor.png");
        float ph = _ptero.mesh.boundsMax().y - _ptero.mesh.boundsMin().y;
        _ptero.scale = (ph > 0.001f) ? (TILE_F * 0.38f) / ph : 0.01f;
        for (int i = 0; i < 6; ++i) {
            WildlifeEntity e;
            glm::vec2 p = spawnXZ(spawnR);
            e.minY   = TILE_F * 3.0f;
            e.maxY   = TILE_F * 5.5f;
            e.pos    = { p.x, wfrand(e.minY, e.maxY), p.y };
            e.target = e.pos;
            e.speed  = wfrand(TILE_F * 0.9f, TILE_F * 1.5f);
            e.animTime = wfrand(0.f, 3.f);
            _ptero.entities.push_back(e);
        }
    } catch (std::exception& e) { std::cerr << "Ptero load: " << e.what() << "\n"; }

    try {
        _butterfly.mesh = SkinnedMesh::load("assets/wildlife/butterfly/scene.gltf");
        _butterfly.mesh.loadTexture("assets/wildlife/butterfly/textures/butterflyMAT_baseColor.png");

        glm::vec3 bsz = _butterfly.mesh.boundsMax() - _butterfly.mesh.boundsMin();
        float span = std::max(bsz.x, bsz.z);
        _butterfly.scale     = (span > 0.001f) ? (TILE_F * 0.28f) / span : 0.005f;
        _butterfly.baseRoll  = 90.f;
        _butterfly.alphaCutout = true;
        for (int i = 0; i < 25; ++i) {
            WildlifeEntity e;
            glm::vec2 p = spawnXZ(spawnR);
            e.minY   = TILE_F * 0.10f;
            e.maxY   = TILE_F * 0.55f;
            e.pos    = { p.x, wfrand(e.minY, e.maxY), p.y };
            e.target = e.pos;
            e.speed  = wfrand(TILE_F * 0.5f, TILE_F * 1.1f);
            e.animTime = wfrand(0.f, 2.f);
            _butterfly.entities.push_back(e);
        }
    } catch (std::exception& e) { std::cerr << "Butterfly load: " << e.what() << "\n"; }
}

void WildlifePass::draw(const FrameContext &f)
{
    float radius = worldExtentRadius(1, _world.map.w * TILE * 0.455f);
    updateWildlife(_ptero,     f.dt, _world.cx, _world.cz, radius);
    updateWildlife(_butterfly, f.dt, _world.cx, _world.cz, radius);
    drawWildlife(_ptero,     _skin, f.view, f.proj, f.camPos, f.skySunDir, f.lightCol,
                 f.ambScale, f.elapsed);
    drawWildlife(_butterfly, _skin, f.view, f.proj, f.camPos, f.skySunDir, f.lightCol,
                 f.ambScale, f.elapsed);
}
