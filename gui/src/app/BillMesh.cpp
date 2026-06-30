#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "app/BillMesh.hpp"
#include "render/Shader.hpp"
#include "AssetCache.hpp"
#include <tiny_gltf.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstring>

static glm::mat4 nodeTRS(const tinygltf::Node& n)
{
    if (!n.matrix.empty()) {
        glm::mat4 m;
        for (int i = 0; i < 16; ++i) m[i/4][i%4] = (float)n.matrix[i];
        return m;
    }
    glm::mat4 m(1.f);
    if (n.translation.size() == 3)
        m = glm::translate(m, {(float)n.translation[0],(float)n.translation[1],(float)n.translation[2]});
    if (n.rotation.size() == 4) {
        glm::quat q((float)n.rotation[3],(float)n.rotation[0],(float)n.rotation[1],(float)n.rotation[2]);
        m = m * glm::mat4_cast(q);
    }
    if (n.scale.size() == 3)
        m = glm::scale(m, {(float)n.scale[0],(float)n.scale[1],(float)n.scale[2]});
    return m;
}

static const float* fptr(const tinygltf::Model& mdl, int acc)
{
    auto& a = mdl.accessors[acc]; auto& bv = mdl.bufferViews[a.bufferView];
    return reinterpret_cast<const float*>(mdl.buffers[bv.buffer].data.data() + bv.byteOffset + a.byteOffset);
}

static std::vector<unsigned int> getIdx(const tinygltf::Model& mdl, int acc)
{
    auto& a  = mdl.accessors[acc]; auto& bv = mdl.bufferViews[a.bufferView];
    const uint8_t* raw = mdl.buffers[bv.buffer].data.data() + bv.byteOffset + a.byteOffset;
    std::vector<unsigned int> out(a.count);
    for (size_t i = 0; i < a.count; ++i) {
        if      (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            out[i] = reinterpret_cast<const uint32_t*>(raw)[i];
        else if (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            out[i] = reinterpret_cast<const uint16_t*>(raw)[i];
        else
            out[i] = raw[i];
    }
    return out;
}

static void walkNode(const tinygltf::Model& mdl, int ni, glm::mat4 parent,
                     std::vector<BillMesh::Part>& out)
{
    const auto& node = mdl.nodes[ni];
    glm::mat4 world  = parent * nodeTRS(node);
    glm::mat3 norm3  = glm::mat3(glm::transpose(glm::inverse(world)));

    if (node.mesh >= 0) {
        for (const auto& prim : mdl.meshes[node.mesh].primitives) {
            auto pIt = prim.attributes.find("POSITION");
            auto nIt = prim.attributes.find("NORMAL");
            if (pIt == prim.attributes.end()) continue;

            glm::vec3 color(0.8f);
            if (prim.material >= 0 && prim.material < (int)mdl.materials.size()) {
                const auto& bc = mdl.materials[prim.material].pbrMetallicRoughness.baseColorFactor;
                if (bc.size() >= 3) color = {(float)bc[0],(float)bc[1],(float)bc[2]};
            }

            size_t vc = mdl.accessors[pIt->second].count;
            const float* pos = fptr(mdl, pIt->second);
            const float* nor = (nIt != prim.attributes.end()) ? fptr(mdl, nIt->second) : nullptr;

            std::vector<float> verts(vc * 9);
            for (size_t i = 0; i < vc; ++i) {
                glm::vec4 wp = world * glm::vec4(pos[i*3],pos[i*3+1],pos[i*3+2],1.f);
                verts[i*9+0]=wp.x; verts[i*9+1]=wp.y; verts[i*9+2]=wp.z;
                glm::vec3 n3(0,1,0);
                if (nor) n3 = glm::normalize(norm3 * glm::vec3(nor[i*3],nor[i*3+1],nor[i*3+2]));
                verts[i*9+3]=n3.x; verts[i*9+4]=n3.y; verts[i*9+5]=n3.z;
                verts[i*9+6]=color.r; verts[i*9+7]=color.g; verts[i*9+8]=color.b;
            }

            std::vector<unsigned int> idx;
            if (prim.indices >= 0) idx = getIdx(mdl, prim.indices);
            else { idx.resize(vc); for (size_t i=0;i<vc;++i) idx[i]=(unsigned int)i; }

            out.push_back({Mesh(verts, idx), color});
        }
    }

    for (int ch : node.children) walkNode(mdl, ch, world, out);
}

bool BillMesh::init(const std::string& path)
{
    std::string bytes = zappy::assetBytes(path);
    if (bytes.empty()) { std::cerr << "[bill] " << path << " not found\n"; return false; }

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader([](tinygltf::Image*,const int,std::string*,std::string*,
                             int,int,const unsigned char*,int,void*){return true;}, nullptr);
    tinygltf::Model model;
    std::string err, warn;
    if (!loader.LoadBinaryFromMemory(&model, &err, &warn,
            reinterpret_cast<const unsigned char*>(bytes.data()),
            (unsigned int)bytes.size(), "")) {
        std::cerr << "[bill] " << err << "\n"; return false;
    }

    for (const auto& scene : model.scenes)
        for (int ni : scene.nodes)
            walkNode(model, ni, glm::mat4(1.f), _parts);

    _ready = !_parts.empty();
    std::cout << "[bill] loaded " << _parts.size() << " parts\n";
    return _ready;
}

void BillMesh::draw(Shader& toon,
                    const glm::mat4& model, const glm::mat4& view,
                    const glm::mat4& proj,  const glm::vec3& camPos,
                    const glm::vec3& sunDir, const glm::vec3& lightCol,
                    float ambScale)
{
    if (!_ready) return;
    glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(model)));
    toon.use();
    toon.setMat4("uView",  glm::value_ptr(view));
    toon.setMat4("uProj",  glm::value_ptr(proj));
    toon.setMat4("uModel", glm::value_ptr(model));
    toon.setMat3("uNormalMat", glm::value_ptr(N3));
    toon.setVec3("uCamPos",        camPos.x, camPos.y, camPos.z);
    toon.setVec3("uDirLightDir",   sunDir.x, sunDir.y, sunDir.z);
    toon.setVec3("uDirLightColor", lightCol.x, lightCol.y, lightCol.z);
    toon.setVec3("uPointLight0Color", 0.f,0.f,0.f);
    toon.setVec3("uPointLight1Color", 0.f,0.f,0.f);
    toon.setFloat("uShininess",      24.f);
    toon.setFloat("uAmbientStrength",0.45f * ambScale);
    toon.setFloat("uAlpha",          1.f);
    toon.setBool ("uEmissive",       false);
    toon.setBool ("uCrystal",        false);
    toon.setBool ("uWave",           false);
    toon.setBool ("uSway",           false);
    toon.setBool ("uCircleClip",     false);
    toon.setBool ("uShoreBlend",     false);
    toon.setBool ("uUseTexture",     false);
    toon.setBool ("uAlphaCutout",    false);
    toon.setBool ("uMultiColor",     false);
    toon.setBool ("uUseVertexColor", true);
    toon.setVec3 ("uColor",          1.f,1.f,1.f);
    glDisable(GL_CULL_FACE);
    for (auto& p : _parts) p.mesh.draw();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}
