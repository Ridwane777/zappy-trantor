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

void saveCache(const std::string& glbPath, const SkinnedMesh& sm,
               const std::vector<float>& verts,
               const std::vector<unsigned int>& indices)
{
    return;   // streaming mode : on n'écrit RIEN sur le disque (assets en RAM only)
    std::ofstream f(cachePath(glbPath), std::ios::binary);
    if (!f) return;
    cw(f, CACHE_MAGIC);
    cw(f, CACHE_VERSION);
    cw(f, sm.bmin_.x); cw(f, sm.bmin_.y); cw(f, sm.bmin_.z);
    cw(f, sm.bmax_.x); cw(f, sm.bmax_.y); cw(f, sm.bmax_.z);
    cwv(f, verts);
    cwv(f, indices);
    uint32_t nj = (uint32_t)sm.invBindMats_.size();
    cw(f, nj);
    for (auto& m : sm.invBindMats_)
        f.write(reinterpret_cast<const char*>(glm::value_ptr(m)), 64);
    cwv(f, sm.parentOf_);
    saveAnimsToCache(f, sm.animations_);
    // bone names
    uint32_t nb = (uint32_t)sm.boneNames_.size();
    cw(f, nb);
    for (auto& name : sm.boneNames_) {
        uint32_t nl = (uint32_t)name.size();
        cw(f, nl);
        f.write(name.c_str(), nl);
    }
}

static bool tryLoadAnimCache(const std::string& glbPath, std::vector<Animation>& anims)
{
    auto cp = cachePath(glbPath);
    struct stat gs{}, cs{};
    if (stat(glbPath.c_str(), &gs) != 0) return false;
    if (stat(cp.c_str(), &cs) != 0) return false;
    if (cs.st_mtime < gs.st_mtime) return false;

    std::ifstream f(cp, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0;
    cr(f, magic); cr(f, version);
    if (magic != CACHE_MAGIC || version != CACHE_VERSION) return false;
    // skip bounds, verts, indices, skin
    glm::vec3 dummy3;
    f.read(reinterpret_cast<char*>(&dummy3), 12); // bmin
    f.read(reinterpret_cast<char*>(&dummy3), 12); // bmax
    uint32_t n = 0;
    cr(f, n); f.seekg(n * 4 * sizeof(float), std::ios::cur); // verts
    cr(f, n); f.seekg(n * sizeof(uint32_t), std::ios::cur);  // indices
    uint32_t nj = 0; cr(f, nj);
    f.seekg(nj * 64, std::ios::cur);              // invBindMats
    cr(f, n); f.seekg(n * sizeof(int32_t), std::ios::cur);   // parents
    return loadAnimsFromCache(f, anims) && f.good();
}

static void saveAnimCache(const std::string& glbPath, const std::vector<Animation>& anims)
{
    // anim-only cache: just magic + version + single animation block
    // For simplicity, use a separate .glbca file
    auto cp = glbPath + ".glbca";
    std::ofstream f(cp, std::ios::binary);
    if (!f) return;
    cw(f, CACHE_MAGIC);
    uint32_t ver2 = CACHE_VERSION + 100; // different tag for anim-only
    cw(f, ver2);
    saveAnimsToCache(f, anims);
}

static bool tryLoadAnimOnlyCache(const std::string& glbPath, std::vector<Animation>& out)
{
    auto cp = glbPath + ".glbca";
    struct stat gs{}, cs{};
    if (stat(glbPath.c_str(), &gs) != 0) return false;
    if (stat(cp.c_str(), &cs) != 0) return false;
    if (cs.st_mtime < gs.st_mtime) return false;
    std::ifstream f(cp, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0;
    cr(f, magic); cr(f, version);
    if (magic != CACHE_MAGIC || version != CACHE_VERSION + 100) return false;
    return loadAnimsFromCache(f, out) && f.good();
}

// ── SkinnedMesh::load ──────────────────────────────────────────────────────────

// tinygltf reads a .gltf's external companions (the .bin buffer, image textures)
// through these callbacks — we route them to the CDN/memory so nothing touches
// the disk. A .gltf is just an index file; the geometry/pixels live in sidecars.
// Base directory of the glTF currently being parsed (e.g. "assets/screen/phoenix/").
// tinygltf may hand the FsCallbacks a *relative* URI ("scene.bin"); we resolve it
// against this. thread_local: each load runs the callbacks on its own thread.
static thread_local std::string g_gltfBaseDir;

static void setCdnFs(tinygltf::TinyGLTF& loader)
{
    tinygltf::FsCallbacks fs;
    fs.FileExists     = [](const std::string&, void*) { return true; };
    fs.ExpandFilePath = [](const std::string& p, void*) { return p; };
    fs.ReadWholeFile  = [](std::vector<unsigned char>* out, std::string* err,
                           const std::string& fp, void*) {
        std::string b = zappy::assetBytes(fp);          // try as given (may be absolute)
        if (b.empty() && fp.rfind("assets/", 0) != 0)   // relative URI -> prepend base dir
            b = zappy::assetBytes(g_gltfBaseDir + fp);
        if (b.empty()) { if (err) *err = "cdn read failed: " + fp; return false; }
        out->assign(b.begin(), b.end());
        return true;
    };
    fs.WriteWholeFile = [](std::string*, const std::string&,
                           const std::vector<unsigned char>&, void*) { return false; };
    loader.SetFsCallbacks(fs);
}

static std::string base64(const std::string& in)
{
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        unsigned n = (unsigned char)in[i] << 16 | (unsigned char)in[i+1] << 8 | (unsigned char)in[i+2];
        out += T[(n>>18)&63]; out += T[(n>>12)&63]; out += T[(n>>6)&63]; out += T[n&63];
    }
    if (i < in.size()) {
        unsigned n = (unsigned char)in[i] << 16;
        if (i + 1 < in.size()) n |= (unsigned char)in[i+1] << 8;
        out += T[(n>>18)&63]; out += T[(n>>12)&63];
        out += (i + 1 < in.size()) ? T[(n>>6)&63] : '=';
        out += '=';
    }
    return out;
}

// For an ASCII .gltf, inline its external buffers/images as base64 data URIs so
// the model is fully self-contained — tinygltf then needs zero external file
// access (its base_dir handling for streamed memory is unreliable). The sidecar
// bytes are fetched from the CDN/RAM store.
static std::string embedGltfExternals(const std::string& gltf, const std::string& dir)
{
    json j = json::parse(gltf, nullptr, false);
    if (j.is_discarded()) return gltf;
    auto embed = [&](const char* key) {
        if (!j.contains(key) || !j[key].is_array()) return;
        for (auto& e : j[key]) {
            if (!e.contains("uri") || !e["uri"].is_string()) continue;
            std::string uri = e["uri"].get<std::string>();
            if (uri.empty() || uri.rfind("data:", 0) == 0) continue;
            std::string bytes = zappy::assetBytes(dir + uri);
            if (bytes.empty()) continue;
            e["uri"] = "data:application/octet-stream;base64," + base64(bytes);
        }
    };
    embed("buffers");
    embed("images");
    return j.dump();
}

// Parse a glTF (.glb binary or .gltf ASCII) straight from its bytes — fetched
// in memory, never written to disk. For ASCII, external sidecars are inlined.
static bool loadGltfModel(const std::string& path, tinygltf::Model& model,
                          std::string& err, std::string& warn)
{
    std::string bytes = zappy::assetBytes(path);
    if (bytes.empty()) return false;
    std::string dir = path.substr(0, path.find_last_of('/') + 1);
    g_gltfBaseDir = dir;                            // fallback for FsCallbacks
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader([](tinygltf::Image*, const int, std::string*, std::string*,
                             int, int, const unsigned char*, int, void*) { return true; }, nullptr);
    setCdnFs(loader);
    bool glb = path.size() > 4 && path.substr(path.size() - 4) == ".glb";
    if (glb)
        return loader.LoadBinaryFromMemory(&model, &err, &warn,
                   reinterpret_cast<const unsigned char*>(bytes.data()),
                   (unsigned int)bytes.size(), dir);
    std::string ascii = embedGltfExternals(bytes, dir);
    return loader.LoadASCIIFromString(&model, &err, &warn,
               ascii.data(), (unsigned int)ascii.size(), dir);
}

SkinnedMesh SkinnedMesh::load(const std::string& path)
{
    SkinnedMesh sm;

    // ── fast path: binary cache ───────────────────────────────────────────────
    {
        std::vector<float>        verts;
        std::vector<unsigned int> indices;
        if (tryLoadCache(path, sm, verts, indices)) {
            sm.upload(verts, indices);
            std::cout << "SkinnedMesh (cache) " << path
                      << "  v=" << verts.size()/16
                      << "  f=" << indices.size()/3
                      << "  joints=" << sm.invBindMats_.size() << "\n";
            for (auto& a : sm.animations_)
                std::cout << "  + anim '" << a.name << "' dur=" << a.duration << "s\n";
            return sm;
        }
    }

    tinygltf::Model model;
    std::string err, warn;
    bool ok = loadGltfModel(path, model, err, warn);

    if (!ok) throw GUIException("tinygltf: " + err);
    if (!warn.empty()) std::cerr << "tinygltf warn: " << warn << "\n";

    // ── skin ──────────────────────────────────────────────────────────────────
    int numJoints = 0;
    if (!model.skins.empty()) {
        auto& skin = model.skins[0];
        numJoints  = (int)skin.joints.size();
        sm.parentOf_.resize(numJoints, -1);

        // inverse bind matrices
        if (skin.inverseBindMatrices >= 0) {
            const float* ibm = floatPtr(model, skin.inverseBindMatrices);
            sm.invBindMats_.resize(numJoints);
            for (int i = 0; i < numJoints; ++i)
                memcpy(glm::value_ptr(sm.invBindMats_[i]), ibm + i*16, 64);
        } else {
            sm.invBindMats_.assign(numJoints, glm::mat4(1.f));
        }

        // parent lookup via node children + store bone names
        sm.boneNames_.resize(numJoints);
        for (int j = 0; j < numJoints; ++j) {
            int nodeIdx = skin.joints[j];
            sm.boneNames_[j] = model.nodes[nodeIdx].name;
            for (auto& child : model.nodes[nodeIdx].children) {
                auto it = std::find(skin.joints.begin(), skin.joints.end(), child);
                if (it != skin.joints.end())
                    sm.parentOf_[it - skin.joints.begin()] = j;
            }
        }
    }

    // ── mesh geometry ─────────────────────────────────────────────────────────
    // vertex layout: pos(3) normal(3) joints(4) weights(4) uv(2) → 16 floats
    std::vector<float>        verts;
    std::vector<unsigned int> indices;

    for (auto& mesh : model.meshes) {
        for (auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            auto posIt = prim.attributes.find("POSITION");
            auto nrmIt = prim.attributes.find("NORMAL");
            auto j0It  = prim.attributes.find("JOINTS_0");
            auto w0It  = prim.attributes.find("WEIGHTS_0");
            auto uvIt  = prim.attributes.find("TEXCOORD_0");
            if (posIt == prim.attributes.end()) continue;

            auto& posAcc = model.accessors[posIt->second];
            size_t nv = posAcc.count;
            const float* pos = floatPtr(model, posIt->second);
            const float* nrm = (nrmIt != prim.attributes.end())
                               ? floatPtr(model, nrmIt->second) : nullptr;
            const float* uv  = (uvIt  != prim.attributes.end())
                               ? floatPtr(model, uvIt->second)  : nullptr;

            std::vector<float> jvec(nv*4, 0.f), wvec(nv*4, 0.f);
            if (j0It != prim.attributes.end()) {
                auto& jAcc = model.accessors[j0It->second];
                if (jAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    auto& bv  = model.bufferViews[jAcc.bufferView];
                    auto& buf = model.buffers[bv.buffer];
                    const uint8_t* jb = buf.data.data() + bv.byteOffset + jAcc.byteOffset;
                    for (size_t i = 0; i < nv*4; ++i) jvec[i] = (float)jb[i];
                } else {
                    const uint16_t* ju = u16Ptr(model, j0It->second);
                    for (size_t i = 0; i < nv*4; ++i) jvec[i] = (float)ju[i];
                }
            }
            if (w0It != prim.attributes.end()) {
                const float* wf = floatPtr(model, w0It->second);
                for (size_t i = 0; i < nv*4; ++i) wvec[i] = wf[i];
            }
            // non-skinned primitive: bind every vertex fully to bone 0 (identity)
            // so the GPU skinning leaves the mesh in place instead of collapsing it
            if (j0It == prim.attributes.end() && w0It == prim.attributes.end()) {
                for (size_t i = 0; i < nv; ++i) wvec[i*4+0] = 1.f;
            }

            unsigned int base = (unsigned int)(verts.size() / 16);
            for (size_t i = 0; i < nv; ++i) {
                verts.push_back(pos[i*3+0]); verts.push_back(pos[i*3+1]); verts.push_back(pos[i*3+2]);
                if (nrm) { verts.push_back(nrm[i*3+0]); verts.push_back(nrm[i*3+1]); verts.push_back(nrm[i*3+2]); }
                else      { verts.push_back(0); verts.push_back(1); verts.push_back(0); }
                verts.push_back(jvec[i*4+0]); verts.push_back(jvec[i*4+1]);
                verts.push_back(jvec[i*4+2]); verts.push_back(jvec[i*4+3]);
                verts.push_back(wvec[i*4+0]); verts.push_back(wvec[i*4+1]);
                verts.push_back(wvec[i*4+2]); verts.push_back(wvec[i*4+3]);
                if (uv) { verts.push_back(uv[i*2+0]); verts.push_back(uv[i*2+1]); }
                else    { verts.push_back(0.f); verts.push_back(0.f); }

                sm.bmin_.x = std::min(sm.bmin_.x, pos[i*3+0]);
                sm.bmin_.y = std::min(sm.bmin_.y, pos[i*3+1]);
                sm.bmin_.z = std::min(sm.bmin_.z, pos[i*3+2]);
                sm.bmax_.x = std::max(sm.bmax_.x, pos[i*3+0]);
                sm.bmax_.y = std::max(sm.bmax_.y, pos[i*3+1]);
                sm.bmax_.z = std::max(sm.bmax_.z, pos[i*3+2]);
            }

            auto primIdx = getIndices(model, prim.indices);
            for (auto idx : primIdx) indices.push_back(base + idx);
        }
    }


    std::cout << "SkinnedMesh " << path
              << "  v=" << verts.size()/16
              << "  f=" << indices.size()/3
              << "  joints=" << numJoints << "\n";

    // ── animations ────────────────────────────────────────────────────────────
    if (!model.skins.empty()) {
        auto& skin = model.skins[0];
        for (auto& anim : model.animations) {
            Animation a;
            a.name = anim.name;
            a.duration = 0.f;

            for (auto& ch : anim.channels) {
                if (ch.target_node < 0) continue;
                // map node → joint index
                auto it = std::find(skin.joints.begin(), skin.joints.end(), ch.target_node);
                if (it == skin.joints.end()) continue;
                int ji = (int)(it - skin.joints.begin());

                auto& samp = anim.samplers[ch.sampler];
                const float* times = floatPtr(model, samp.input);
                size_t       nk    = model.accessors[samp.input].count;
                const float* vals  = floatPtr(model, samp.output);

                // find or create channel for this joint
                BoneChannel* bc = nullptr;
                for (auto& c : a.channels)
                    if (c.boneIndex == ji) { bc = &c; break; }
                if (!bc) { a.channels.push_back({ji}); bc = &a.channels.back(); }

                if (nk > 0) a.duration = std::max(a.duration, times[nk-1]);

                if (ch.target_path == "translation") {
                    bc->timesT.assign(times, times+nk);
                    bc->transKeys.resize(nk);
                    for (size_t i = 0; i < nk; ++i)
                        bc->transKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
                } else if (ch.target_path == "rotation") {
                    bc->timesR.assign(times, times+nk);
                    bc->rotKeys.resize(nk);
                    for (size_t i = 0; i < nk; ++i)
                        // GLTF quat is x,y,z,w; glm is w,x,y,z
                        bc->rotKeys[i] = glm::quat(vals[i*4+3], vals[i*4], vals[i*4+1], vals[i*4+2]);
                } else if (ch.target_path == "scale") {
                    bc->timesS.assign(times, times+nk);
                    bc->scaleKeys.resize(nk);
                    for (size_t i = 0; i < nk; ++i)
                        bc->scaleKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
                }
            }
            sm.animations_.push_back(std::move(a));
        }
    }

    sm.upload(verts, indices);
    saveCache(path, sm, verts, indices);
    return sm;
}

// ── load extra animation from a second GLB (same skeleton) ───────────────────

void SkinnedMesh::loadAnimFrom(const std::string& path)
{
    // ── fast path: anim-only cache ────────────────────────────────────────────
    {
        std::vector<Animation> loaded;
        if (tryLoadAnimOnlyCache(path, loaded)) {
            for (auto& a : loaded) {
                animations_.push_back(std::move(a));
                std::cout << "  + anim (cache) '" << animations_.back().name
                          << "' dur=" << animations_.back().duration << "s\n";
            }
            return;
        }
    }

    tinygltf::Model model;
    std::string err, warn;
    bool ok = loadGltfModel(path, model, err, warn);
    if (!ok) { std::cerr << "loadAnimFrom failed: " << err << "\n"; return; }

    if (model.skins.empty()) return;
    auto& skin = model.skins[0];

    for (auto& anim : model.animations) {
        Animation a;
        a.name = anim.name.empty() ? path : anim.name;
        a.duration = 0.f;

        for (auto& ch : anim.channels) {
            if (ch.target_node < 0) continue;
            auto it = std::find(skin.joints.begin(), skin.joints.end(), ch.target_node);
            if (it == skin.joints.end()) continue;
            int ji = (int)(it - skin.joints.begin());

            auto& samp = anim.samplers[ch.sampler];
            const float* times = floatPtr(model, samp.input);
            size_t       nk    = model.accessors[samp.input].count;
            const float* vals  = floatPtr(model, samp.output);

            BoneChannel* bc = nullptr;
            for (auto& c : a.channels)
                if (c.boneIndex == ji) { bc = &c; break; }
            if (!bc) { a.channels.push_back({ji}); bc = &a.channels.back(); }

            if (nk > 0) a.duration = std::max(a.duration, times[nk-1]);

            if (ch.target_path == "translation") {
                bc->timesT.assign(times, times+nk);
                bc->transKeys.resize(nk);
                for (size_t i = 0; i < nk; ++i)
                    bc->transKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
            } else if (ch.target_path == "rotation") {
                bc->timesR.assign(times, times+nk);
                bc->rotKeys.resize(nk);
                for (size_t i = 0; i < nk; ++i)
                    bc->rotKeys[i] = glm::quat(vals[i*4+3], vals[i*4], vals[i*4+1], vals[i*4+2]);
            } else if (ch.target_path == "scale") {
                bc->timesS.assign(times, times+nk);
                bc->scaleKeys.resize(nk);
                for (size_t i = 0; i < nk; ++i)
                    bc->scaleKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
            }
        }
        animations_.push_back(std::move(a));
        std::cout << "  + anim '" << animations_.back().name
                  << "' dur=" << animations_.back().duration << "s\n";
    }

    // collect only the newly-added animations and write anim-only cache
    std::vector<Animation> newAnims(animations_.end() - (int)model.animations.size(), animations_.end());
    saveAnimCache(path, newAnims);
}

// ── bone evaluation ───────────────────────────────────────────────────────────

void SkinnedMesh::computeBones(int animIdx, float t,
                               glm::mat4 boneMats[MAX_BONES],
                               bool loop, bool reversed) const
{
