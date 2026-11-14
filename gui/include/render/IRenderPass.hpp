#pragma once

#include <glm/glm.hpp>

class INetworkClient;

// Everything a render pass may read about the current frame. Built once per
// frame by the application loop and handed to every pass in order.
struct FrameContext {
    glm::mat4 view{1.f};
    glm::mat4 proj{1.f};
    glm::vec3 camPos{0.f};

    // Lighting from the day/night cycle.
    glm::vec3 sunDir{0.f, 1.f, 0.f};
    glm::vec3 skySunDir{0.f, 1.f, 0.f};
    glm::vec3 moonDir{0.f, 1.f, 0.f};
    glm::vec3 lightCol{1.f};
    glm::vec3 skyTop{0.f};
    glm::vec3 skyHor{0.f};
    float     nightF   = 0.f;
    float     ambScale = 1.f;

    float elapsed = 0.f;  // seconds since the game scene started
    float dt      = 0.f;  // seconds since the previous frame

    // Server view, read-only. Null while offline.
    const INetworkClient *net = nullptr;

    bool showTileGrid = false;
};

// One step of the 3D scene. Passes are drawn in sequence each frame; a pass
// may rely on GL state (shared shader uniforms) set by an earlier pass.
class IRenderPass {
public:
    virtual ~IRenderPass() = default;
    virtual void draw(const FrameContext &frame) = 0;
};
