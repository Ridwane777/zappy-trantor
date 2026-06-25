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
