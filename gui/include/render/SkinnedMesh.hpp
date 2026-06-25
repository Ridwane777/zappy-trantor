#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

static const int MAX_BONES = 96;

struct BoneChannel {
    int   boneIndex;
    std::vector<float>     timesT, timesR, timesS;
    std::vector<glm::vec3> transKeys;
    std::vector<glm::quat> rotKeys;
    std::vector<glm::vec3> scaleKeys;
};

struct Animation {
    std::string            name;
    float                  duration;   // seconds
    std::vector<BoneChannel> channels;
};

struct LocalTRS {
    glm::vec3 t{0,0,0};
    glm::quat r{1,0,0,0};
    glm::vec3 s{1,1,1};
};

class SkinnedMesh {
public:
    SkinnedMesh() = default;
    ~SkinnedMesh();

    // non-copyable
    SkinnedMesh(const SkinnedMesh&)            = delete;
    SkinnedMesh& operator=(const SkinnedMesh&) = delete;
    SkinnedMesh(SkinnedMesh&&) noexcept;
    SkinnedMesh& operator=(SkinnedMesh&&) noexcept;

    static SkinnedMesh load(const std::string& path);

    void loadVertexColors(const std::string& colPath);
    void loadTexture(const std::string& pngPath);
    void loadAnimFrom(const std::string& path);

    void computeBones(int animIndex, float t, glm::mat4 boneMats[MAX_BONES],
                      bool loop = true, bool reversed = false) const;

    void draw() const;

    int   animCount()                  const { return (int)animations_.size(); }
    const std::string& animName(int i) const { return animations_[i].name; }
    float animDuration(int i)          const { return animations_[i].duration; }

    glm::vec3 boundsMin() const { return bmin_; }
    glm::vec3 boundsMax() const { return bmax_; }

    int  boneCount()                            const { return (int)invBindMats_.size(); }
    int  boneIndexByName(const std::string& n)  const;
    void addAnimation(Animation a);

    // Returns per-bone local TRS sampled from animation animIdx at time t.
    std::vector<LocalTRS> extractLocalTRS(int animIdx, float t) const;

    // Apply localRot to a bone and propagate the world-space delta to all
    // its descendants. Call after computeBones to overlay procedural motion.
