#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include "render/Mesh.hpp"

class BillMesh {
public:
    bool init(const std::string& glbPath);
    bool ready() const { return _ready; }

    void draw(class Shader& toon,
              const glm::mat4& model, const glm::mat4& view,
              const glm::mat4& proj,  const glm::vec3& camPos,
              const glm::vec3& sunDir, const glm::vec3& lightCol,
              float ambScale);

private:
    struct Part { Mesh mesh; glm::vec3 color; };
    std::vector<Part> _parts;
    bool _ready = false;
};
