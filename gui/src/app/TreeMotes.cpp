#include "app/TreeMotes.hpp"
#include "render/Shader.hpp"
#include "world/World.hpp"        // frnd(a, b)

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>

void drawTreeMotes(std::vector<Spark>& motes, bool charging, float chargeT,
                   bool justReleased, const glm::vec3& treePos,
                   Shader& star, unsigned vao, const glm::mat4& view,
                   const glm::mat4& proj, float elapsed, float dt)
{
    const float baseR = (16.6667f / 1.381f) * 0.8f;

    // ── CHARGING: motes gather and spiral up the trunk (energy building) ─
    if (charging) {
        int n = (int)(chargeT * chargeT * 7.f) + (frnd(0.f, 1.f) < chargeT ? 1 : 0);
        for (int i = 0; i < n; ++i) {
            Spark m;
            float ang = frnd(0.f, 6.2832f);
