#pragma once

#include <memory>
#include <vector>

#include "render/IRenderPass.hpp"
#include "render/Mesh.hpp"
#include "render/TexturedMesh.hpp"
#include "app/Character.hpp"
#include "app/IncantationFx.hpp"
#include "app/Particle.hpp"
#include "app/PlayerRender.hpp"
#include "app/Wildlife.hpp"

class Shader;
class MeshCache;
class HudOverlay;
class Constellation;
struct MapData;

// The island shown by the passes. The application may replace it (map hot
// reload), so passes keep references to its storage rather than copies.
struct WorldRefs {
    const MapData &map;
    const float   &cx;
    const float   &cz;
};

// Full-screen gradient sky with sun and moon discs.
class SkyPass : public IRenderPass {
public:
    explicit SkyPass(Shader &sky);
    ~SkyPass() override;
    SkyPass(const SkyPass &) = delete;
    SkyPass &operator=(const SkyPass &) = delete;
    void draw(const FrameContext &frame) override;

private:
    Shader      &_sky;
    unsigned int _vao = 0, _vbo = 0, _ebo = 0;
};

// Terrain, ocean, shore water and map props. Also sets the toon shader's
// per-frame uniforms that the following passes reuse.
class WorldPass : public IRenderPass {
public:
    WorldPass(Shader &toon, Shader &outline, Shader &waterMap, MeshCache &meshes,
              WorldRefs world, Mesh &land, Mesh &ocean, Mesh &water,
              const float &treeGlow);
    ~WorldPass() override;
    WorldPass(const WorldPass &) = delete;
    WorldPass &operator=(const WorldPass &) = delete;
    void draw(const FrameContext &frame) override;

private:
    Shader &_toon, &_outline, &_waterMap;
    MeshCache &_meshes;
    WorldRefs _world;
    Mesh &_land, &_ocean, &_water;
    const float &_treeGlow;
    unsigned int _waterTex = 0;
};

// Gems and food lying on the tiles.
class ResourcePass : public IRenderPass {
public:
    ResourcePass(Shader &egg, WorldRefs world);
    void draw(const FrameContext &frame) override;

private:
    Shader &_egg;
    WorldRefs _world;
    std::unique_ptr<TexturedMesh> _gemTex[7];
    std::unique_ptr<TexturedMesh> _foodMesh[3];
};

// Trantorians (server players or demo players), eggs, mounts, moon, and the
// beam over the selected player.
class PlayerPass : public IRenderPass {
public:
    struct Shaders { Shader &skin; Shader &aura; Shader &toon; Shader &star; Shader &egg; };
    PlayerPass(const PlayerRenderCtx &ctx, Shaders shaders, unsigned sparkVAO,
               std::vector<Character> &characters, const HudOverlay &hud);
    void draw(const FrameContext &frame) override;

private:
    void drawSelectionBeam(const FrameContext &frame);

    PlayerRenderCtx _ctx;
    Shaders _sh;
    unsigned _sparkVAO;
    std::vector<Character> &_characters;
    const HudOverlay &_hud;
    Mesh _beamMesh;
};

// State of the tree's charge/release cycle, written by the update step.
struct TreePulse {
    bool  charging     = false;
    float chargeT      = 0.f;
    bool  justReleased = false;
};

// Glowing motes rising around the tree of life.
class TreeMotesPass : public IRenderPass {
public:
    TreeMotesPass(Shader &star, unsigned sparkVAO, const TreePulse &pulse,
                  const glm::vec3 &treePos);
    void draw(const FrameContext &frame) override;

private:
    Shader &_star;
    unsigned _sparkVAO;
    const TreePulse &_pulse;
    glm::vec3 _treePos;
    std::vector<Spark> _motes;
};

// Energy trail of the admin-mode mount.
class MewEnergyPass : public IRenderPass {
public:
    MewEnergyPass(Constellation &constellation, Shader &star, unsigned sparkVAO,
                  const bool &adminMode, const glm::vec3 &mewPos);
    void draw(const FrameContext &frame) override;

private:
    Constellation &_constellation;
    Shader &_star;
    unsigned _sparkVAO;
    const bool &_adminMode;
    const glm::vec3 &_mewPos;
};

// Elevation rituals in progress.
class IncantationPass : public IRenderPass {
public:
    IncantationPass(Shader &incant, const std::vector<IncantationEffect> &fx);
    void draw(const FrameContext &frame) override;

private:
    Shader &_incant;
    const std::vector<IncantationEffect> &_fx;
};

class CloudPass : public IRenderPass {
public:
    CloudPass(Shader &toon, MeshCache &meshes, const MapData &map);
    void draw(const FrameContext &frame) override;

private:
    Shader &_toon;
    MeshCache &_meshes;
    const MapData &_map;
};

// Debug grid of the server tiles (toggled with G).
class TileGridPass : public IRenderPass {
public:
    explicit TileGridPass(WorldRefs world);
    void draw(const FrameContext &frame) override;

private:
    WorldRefs _world;
};

// Pterosaurs and butterflies wandering over the island.
class WildlifePass : public IRenderPass {
public:
    WildlifePass(Shader &skin, WorldRefs world);
    void draw(const FrameContext &frame) override;

private:
    Shader &_skin;
    WorldRefs _world;
    WildlifeGroup _ptero, _butterfly;
};
