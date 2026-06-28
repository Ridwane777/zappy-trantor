#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

static const int MAX_BONES = 128;

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
    void overlayBoneChain(int rootBoneIdx, const glm::mat4& localRot,
                          glm::mat4 boneMats[MAX_BONES]) const;

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0, colorVbo_ = 0, texId_ = 0;
    GLsizei indexCount_ = 0;

    std::vector<glm::mat4> invBindMats_;
    std::vector<int>       parentOf_;
    std::vector<LocalTRS>  bindLocal_;   // per-joint bind-pose TRS (default when a channel is absent)
    std::vector<std::string> boneNames_;
    glm::mat4              skinRootXform_{1.f}; // ancestor-node transforms above the root joint

    std::vector<Animation> animations_;

    glm::vec3 bmin_{0}, bmax_{0};

    void upload(const std::vector<float>& verts,
                const std::vector<unsigned int>& idx);

    friend bool tryLoadCache(const std::string&, SkinnedMesh&,
                             std::vector<float>&, std::vector<unsigned int>&);
    friend void saveCache(const std::string&, const SkinnedMesh&,
                          const std::vector<float>&, const std::vector<unsigned int>&);
};
