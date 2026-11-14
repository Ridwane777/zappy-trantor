#pragma once

#include <glm/glm.hpp>

class AudioManager;
class SkinnedMesh;
struct GuiConfig;

// Keyboard-driven charizard mount (easter egg): walk, take off, fly, land.
// Reads the arrow keys, Space, Z and S directly while its mode is active.
class CharzController {
public:
    CharzController(const GuiConfig &cfg, const SkinnedMesh &mesh, bool haveMesh,
                    float cx, float cz);

    // Puts the mount in the air in front of a camera looking along `camYaw`.
    void enterFlying(float cx, float cz, float camYaw);
    void update(float dt, AudioManager &gameAudio);

    bool onGround() const { return charzPos.y <= CHARZ_GROUND_Y + 0.05f; }
    // The skeleton root motion is stripped only in the looping states.
    bool stripRoot() const
    { return charzMove == CharzMove::GROUNDED || charzMove == CharzMove::FLYING; }

    // Read by the renderer (through const references) and the follow camera.
    glm::vec3 charzPos;
    float     charzYaw      = 0.f;
    float     charzAnimTime = 0.f;
    int       charzAnimIdx;
    float     charzBobAmp   = 0.f;

private:
    enum class CharzMove { GROUNDED, TAKEOFF, FLYING, LANDING_A, LANDING_B };

    const SkinnedMesh &charzMesh;
    const bool haveCharz;
    const float CHARZ_GROUND_Y;
    const int   CA_IDLE;
    const int   CA_FLY_IDLE;
    const float CHARZ_ANIM_SPEED;
    static constexpr int CA_WALK       = 7;
    static constexpr int CA_RUN        = 8;
    static constexpr int CA_TURN_L     = 4;
    static constexpr int CA_TURN_R     = 5;
    static constexpr int CA_AIR_WALK   = 54;
    static constexpr int CA_AIR_RUN    = 55;
    static constexpr int CA_AIR_TURN_L = 53;
    static constexpr int CA_AIR_TURN_R = 52;
    static constexpr int CA_TAKEOFF    = 12;
    static constexpr int CA_FLY        = 13;
    static constexpr int CA_FALL       = 62;
    static constexpr int CA_LAND_A     = 63;
    static constexpr int CA_LAND_B     = 16;
    const float CHARZ_FLY_Y;
    const float caTakeoffDur;
    const float caClimbDur = 0.9f;
    const float caLandADur;
    const float caLandBDur;

    CharzMove charzMove     = CharzMove::GROUNDED;
    float     charzMoveT    = 0.f;
    int       charzPrevAnim = -1;
};
