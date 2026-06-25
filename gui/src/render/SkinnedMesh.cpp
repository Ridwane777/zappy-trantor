#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "GUIException.hpp"
#include <json.hpp>
using json = nlohmann::json;
#include <tiny_gltf.h>

#include "render/SkinnedMesh.hpp"
#include "AssetCache.hpp"
#include <SFML/Graphics/Image.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

// ── helpers ───────────────────────────────────────────────────────────────────

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static int findInterval(const std::vector<float>& times, float t)
{
    if (times.size() < 2) return 0;
    for (int i = 0; i < (int)times.size() - 1; ++i)
        if (t <= times[i+1]) return i;
    return (int)times.size() - 2;
}

template<class T>
static T sampleLinear(const std::vector<float>& times,
                      const std::vector<T>& keys, float t)
{
    if (keys.empty()) return T{};
    if (keys.size() == 1) return keys[0];
    int i = findInterval(times, t);
    float dt = times[i+1] - times[i];
    float f  = (dt > 1e-6f) ? (t - times[i]) / dt : 0.f;
    return glm::mix(keys[i], keys[i+1], f);
}

static glm::quat sampleSlerp(const std::vector<float>& times,
                              const std::vector<glm::quat>& keys, float t)
{
    if (keys.empty()) return glm::quat(1,0,0,0);
    if (keys.size() == 1) return keys[0];
    int i = findInterval(times, t);
    float dt = times[i+1] - times[i];
    float f  = (dt > 1e-6f) ? (t - times[i]) / dt : 0.f;
    return glm::slerp(keys[i], keys[i+1], f);
}

// ── raw buffer accessor helpers ───────────────────────────────────────────────

static const float* floatPtr(const tinygltf::Model& m, int accIdx)
{
    auto& acc = m.accessors[accIdx];
    auto& bv  = m.bufferViews[acc.bufferView];
    auto& buf = m.buffers[bv.buffer];
    return reinterpret_cast<const float*>(
        buf.data.data() + bv.byteOffset + acc.byteOffset);
}

static const uint16_t* u16Ptr(const tinygltf::Model& m, int accIdx)
{
    auto& acc = m.accessors[accIdx];
    auto& bv  = m.bufferViews[acc.bufferView];
    auto& buf = m.buffers[bv.buffer];
    return reinterpret_cast<const uint16_t*>(
        buf.data.data() + bv.byteOffset + acc.byteOffset);
}

static std::vector<unsigned int> getIndices(const tinygltf::Model& m, int accIdx)
{
    auto& acc = m.accessors[accIdx];
    auto& bv  = m.bufferViews[acc.bufferView];
    auto& buf = m.buffers[bv.buffer];
    const uint8_t* base = buf.data.data() + bv.byteOffset + acc.byteOffset;
    std::vector<unsigned int> out(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        if      (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            out[i] = reinterpret_cast<const uint16_t*>(base)[i];
        else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            out[i] = reinterpret_cast<const unsigned int*>(base)[i];
        else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            out[i] = base[i];
    }
    return out;
}

// ── binary cache helpers ──────────────────────────────────────────────────────

static const uint32_t CACHE_MAGIC   = 0x43424C47; // 'GLBC'
static const uint32_t CACHE_VERSION = 3;

static std::string cachePath(const std::string& glbPath) {
    return glbPath + ".glbc";
}

template<class T>
static void cw(std::ofstream& f, const T& v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(T));
}
template<class T>
static void cwv(std::ofstream& f, const std::vector<T>& v) {
    uint32_t n = (uint32_t)v.size();
    cw(f, n);
    if (n) f.write(reinterpret_cast<const char*>(v.data()), n * sizeof(T));
}
template<class T>
static bool cr(std::ifstream& f, T& v) {
    return (bool)f.read(reinterpret_cast<char*>(&v), sizeof(T));
}
template<class T>
static bool crv(std::ifstream& f, std::vector<T>& v) {
    uint32_t n = 0;
    if (!cr(f, n)) return false;
    v.resize(n);
    if (n) f.read(reinterpret_cast<char*>(v.data()), n * sizeof(T));
    return f.good();
}

static void saveAnimsToCache(std::ofstream& f, const std::vector<Animation>& anims)
{
    uint32_t na = (uint32_t)anims.size();
    cw(f, na);
    for (auto& a : anims) {
        uint32_t nl = (uint32_t)a.name.size();
        cw(f, nl);
        f.write(a.name.c_str(), nl);
        cw(f, a.duration);
        uint32_t nc = (uint32_t)a.channels.size();
        cw(f, nc);
        for (auto& bc : a.channels) {
            cw(f, bc.boneIndex);
            cwv(f, bc.timesT);
            // transKeys as flat float3
            uint32_t nt = (uint32_t)bc.transKeys.size();
            cw(f, nt);
            for (auto& v : bc.transKeys) { cw(f,v.x); cw(f,v.y); cw(f,v.z); }
            cwv(f, bc.timesR);
            uint32_t nr = (uint32_t)bc.rotKeys.size();
            cw(f, nr);
            for (auto& q : bc.rotKeys) { cw(f,q.w); cw(f,q.x); cw(f,q.y); cw(f,q.z); }
            cwv(f, bc.timesS);
            uint32_t ns = (uint32_t)bc.scaleKeys.size();
            cw(f, ns);
            for (auto& v : bc.scaleKeys) { cw(f,v.x); cw(f,v.y); cw(f,v.z); }
        }
    }
}

static bool loadAnimsFromCache(std::ifstream& f, std::vector<Animation>& anims)
{
    uint32_t na = 0; if (!cr(f, na)) return false;
    anims.resize(na);
    for (auto& a : anims) {
        uint32_t nl = 0; cr(f, nl);
        a.name.resize(nl);
        f.read(&a.name[0], nl);
        cr(f, a.duration);
        uint32_t nc = 0; cr(f, nc);
        a.channels.resize(nc);
        for (auto& bc : a.channels) {
            cr(f, bc.boneIndex);
            crv(f, bc.timesT);
            uint32_t nt = 0; cr(f, nt);
            bc.transKeys.resize(nt);
            for (auto& v : bc.transKeys) { cr(f,v.x); cr(f,v.y); cr(f,v.z); }
            crv(f, bc.timesR);
            uint32_t nr = 0; cr(f, nr);
            bc.rotKeys.resize(nr);
            for (auto& q : bc.rotKeys) { cr(f,q.w); cr(f,q.x); cr(f,q.y); cr(f,q.z); }
            crv(f, bc.timesS);
            uint32_t ns = 0; cr(f, ns);
            bc.scaleKeys.resize(ns);
            for (auto& v : bc.scaleKeys) { cr(f,v.x); cr(f,v.y); cr(f,v.z); }
        }
    }
    return f.good();
}

bool tryLoadCache(const std::string& glbPath, SkinnedMesh& sm,
                  std::vector<float>& verts, std::vector<unsigned int>& indices)
{
    auto cp = cachePath(glbPath);
    struct stat gs{}, cs{};
    if (stat(glbPath.c_str(), &gs) != 0) return false;
    if (stat(cp.c_str(), &cs) != 0) return false;
    if (cs.st_mtime < gs.st_mtime) return false;  // GLB newer than cache

    std::ifstream f(cp, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0;
    cr(f, magic); cr(f, version);
    if (magic != CACHE_MAGIC || version != CACHE_VERSION) return false;

    cr(f, sm.bmin_.x); cr(f, sm.bmin_.y); cr(f, sm.bmin_.z);
    cr(f, sm.bmax_.x); cr(f, sm.bmax_.y); cr(f, sm.bmax_.z);
    if (!crv(f, verts))   return false;
    if (!crv(f, indices)) return false;

    uint32_t nj = 0; cr(f, nj);
    sm.invBindMats_.resize(nj);
    for (auto& m : sm.invBindMats_)
        f.read(reinterpret_cast<char*>(glm::value_ptr(m)), 64);
    crv(f, sm.parentOf_);

    if (!loadAnimsFromCache(f, sm.animations_)) return false;
    // bone names
    uint32_t nb = 0; cr(f, nb);
    sm.boneNames_.resize(nb);
    for (auto& name : sm.boneNames_) {
        uint32_t nl = 0; cr(f, nl);
        name.resize(nl);
        if (nl) f.read(&name[0], nl);
    }
    return f.good();
}

