#include "menu/MenuScene.hpp"
#include "AssetCache.hpp"
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "render/ObjLoader.hpp"
#include "render/TexturedMesh.hpp"
#include "render/SkinnedMesh.hpp"
#include "audio/AudioManager.hpp"

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <algorithm>

namespace {

// Where the real logo lives once exported from Meshy (decimated .obj). If it's
// missing we fall back to a procedural block so the menu still runs.
const char *LOGO_PATH = "assets/screen/zappy_logo.obj";
const char *LOGO_TEX  = "assets/screen/zappy_logo.png";

// ── procedural meshes ───────────────────────────────────────────────────────

// UV sphere, interleaved pos+normal (stride 6).
Mesh buildSphere(float radius, int segs, int rings)
{
    std::vector<float>        v;
    std::vector<unsigned int> idx;
    for (int i = 0; i <= rings; ++i) {
        float phi = float(M_PI) * float(i) / float(rings);     // 0..pi
        float y   = std::cos(phi);
        float r   = std::sin(phi);
        for (int j = 0; j <= segs; ++j) {
            float th = 2.f * float(M_PI) * float(j) / float(segs);
            glm::vec3 n(r * std::cos(th), y, r * std::sin(th));
            v.insert(v.end(), {n.x * radius, n.y * radius, n.z * radius,
                               n.x, n.y, n.z});
        }
    }
    int stride = segs + 1;
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < segs; ++j) {
            unsigned int a = i * stride + j;
            unsigned int b = a + stride;
            idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    return Mesh(v, idx);
}

// A chunky placeholder logo (a beveled-ish box) used until the real .obj exists.
Mesh buildPlaceholderLogo()
{
    // Simple box, pos+normal. Wide and short so it reads as a wordmark block.
    const float x = 2.4f, y = 0.6f, z = 0.35f;
    std::vector<float> v;
    std::vector<unsigned int> idx;
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
        unsigned int base = (unsigned int)(v.size() / 6);
        for (auto &p : {a, b, c, d})
            v.insert(v.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };
    glm::vec3 p000(-x, -y, -z), p001(-x, -y, z), p010(-x, y, -z), p011(-x, y, z);
    glm::vec3 p100(x, -y, -z),  p101(x, -y, z),  p110(x, y, -z),  p111(x, y, z);
    quad(p001, p101, p111, p011, {0, 0, 1});    // front
    quad(p100, p000, p010, p110, {0, 0, -1});   // back
    quad(p011, p111, p110, p010, {0, 1, 0});    // top
    quad(p000, p100, p101, p001, {0, -1, 0});   // bottom
    quad(p101, p100, p110, p111, {1, 0, 0});    // right
    quad(p000, p001, p011, p010, {-1, 0, 0});   // left
    return Mesh(v, idx);
}

float frand(float a, float b) { return a + (b - a) * (float(std::rand()) / float(RAND_MAX)); }

struct Particle {
    glm::vec3 pos, vel;
    float life, maxLife, size;
    glm::vec3 color;
    float phase, twFreq;   // per-mote twinkle phase + frequency (uncanny flicker)
};

// ── 2D button helper ────────────────────────────────────────────────────────

// Draw a vertical-gradient quad (top brighter -> bottom darker) for a raised look.
static void drawGradientRect(sf::RenderWindow &w, float x, float y, float ww, float hh,
                             sf::Color top, sf::Color bot)
{
    sf::VertexArray q(sf::Quads, 4);
    q[0].position = {x, y};           q[0].color = top;
    q[1].position = {x + ww, y};      q[1].color = top;
    q[2].position = {x + ww, y + hh}; q[2].color = bot;
    q[3].position = {x, y + hh};      q[3].color = bot;
    w.draw(q);
}

// A raised, bevelled button: drop shadow + gradient body + light top bevel +
// dark bottom bevel + embossed (double-drawn) label.
struct Button {
    sf::Vector2f size;
    sf::Vector2f pos;          // top-left
    sf::Text     label;
    bool hovered = false;

    void setup(sf::Font &font, const std::string &text, float w, float h)
    {
        size = {w, h};
        label.setFont(font);
        label.setString(text);
        label.setCharacterSize(24);
    }
    void place(float cx, float cy)
    {
        pos = {cx - size.x / 2.f, cy - size.y / 2.f};
        sf::FloatRect lb = label.getLocalBounds();
        label.setPosition(cx - lb.width / 2.f - lb.left,
                          cy - lb.height / 2.f - lb.top);
    }
    bool contains(float px, float py) const
    {
        return px >= pos.x && py >= pos.y && px <= pos.x + size.x && py <= pos.y + size.y;
    }
    void draw(sf::RenderWindow &w)
    {
        float x = pos.x, y = pos.y, ww = size.x, hh = size.y;

        // 1) drop shadow (soft, offset down-right)
        sf::RectangleShape shadow({ww, hh});
        shadow.setPosition(x + 5.f, y + 6.f);
        shadow.setFillColor(sf::Color(0, 0, 0, 110));
        w.draw(shadow);

        // 2) gradient body
        sf::Color top = hovered ? sf::Color(86, 116, 180) : sf::Color(52, 66, 98);
        sf::Color bot = hovered ? sf::Color(40, 60, 110)  : sf::Color(22, 28, 46);
        drawGradientRect(w, x, y, ww, hh, top, bot);

        // 3) bevel edges: bright top + left, dark bottom + right
        sf::Color hi = hovered ? sf::Color(170, 205, 255, 230) : sf::Color(120, 150, 200, 200);
        sf::Color lo = sf::Color(0, 0, 0, 150);
        sf::RectangleShape edge;
        edge.setFillColor(hi); edge.setSize({ww, 2.f});  edge.setPosition(x, y);           w.draw(edge); // top
        edge.setSize({2.f, hh}); edge.setPosition(x, y);                                    w.draw(edge); // left
        edge.setFillColor(lo); edge.setSize({ww, 2.f}); edge.setPosition(x, y + hh - 2.f);  w.draw(edge); // bottom
        edge.setSize({2.f, hh}); edge.setPosition(x + ww - 2.f, y);                         w.draw(edge); // right

        // 4) outline
        sf::RectangleShape outline({ww, hh});
        outline.setPosition(x, y);
        outline.setFillColor(sf::Color::Transparent);
        outline.setOutlineThickness(1.f);
        outline.setOutlineColor(hovered ? sf::Color(180, 215, 255) : sf::Color(90, 110, 150));
        w.draw(outline);

        // 5) embossed label: dark shadow under, bright text over
        sf::Vector2f lp = label.getPosition();
        label.setPosition(lp.x + 1.5f, lp.y + 1.5f);
        label.setFillColor(sf::Color(0, 0, 0, 160));
        w.draw(label);
        label.setPosition(lp);
        label.setFillColor(sf::Color(238, 244, 255));
        w.draw(label);
    }
};

// Gear-icon button (settings) that lives in a screen corner.
struct IconButton {
    sf::Sprite sprite;
    sf::Vector2f pos;
    float size = 46.f;
    bool hovered = false;
    bool valid = false;

    void setup(const sf::Texture &tex, float px, float py, float sz)
    {
        sprite.setTexture(tex);
        size = sz;
        pos = {px, py};
        auto ts = tex.getSize();
        if (ts.x > 0 && ts.y > 0) {
            sprite.setScale(sz / float(ts.x), sz / float(ts.y));
            valid = true;
        }
        sprite.setPosition(px, py);
    }
    bool contains(float mx, float my) const
    {
        return mx >= pos.x && my >= pos.y && mx <= pos.x + size && my <= pos.y + size;
    }
    void draw(sf::RenderWindow &w)
    {
        // subtle hover halo
        if (hovered) {
            sf::CircleShape halo(size * 0.62f);
            halo.setOrigin(size * 0.62f, size * 0.62f);
            halo.setPosition(pos.x + size / 2.f, pos.y + size / 2.f);
            halo.setFillColor(sf::Color(120, 160, 230, 60));
            w.draw(halo);
        }
        sprite.setColor(hovered ? sf::Color(190, 220, 255) : sf::Color(150, 170, 205));
        w.draw(sprite);
    }
};

// Common phong uniform defaults so the shader's branches are all initialised.
void setPhongDefaults(Shader &sh)
{
    sh.use();
    sh.setBool("uUseTexture", false);
    sh.setBool("uUseVertexColor", false);
    sh.setBool("uMultiColor", false);
    sh.setBool("uCrystal", false);
    sh.setBool("uWave", false);
    sh.setBool("uSway", false);
    sh.setBool("uCircleClip", false);
    sh.setBool("uShoreBlend", false);
    sh.setFloat("uTime", 0.f);
    sh.setFloat("uShininess", 32.f);
    sh.setFloat("uAmbientStrength", 0.18f);
    sh.setFloat("uAlpha", 1.f);
}

} // namespace

bool runMenu(sf::RenderWindow &window, sf::Font &font, sf::Font &uiFont,
             MenuConfig &out)
{
    Shader phong("shaders/phong.vert", "shaders/phong.frag");
    Shader logoShader("shaders/logo.vert", "shaders/logo.frag");
    Shader starShader("shaders/star.vert", "shaders/star.frag");
    Shader skyShader ("shaders/sky.vert",  "shaders/sky.frag");
    Shader sandShader("shaders/sand.vert", "shaders/sand.frag");
    Shader outline   ("shaders/outline.vert", "shaders/outline.frag");
    Shader skinShader("shaders/skinned.vert", "shaders/phong.frag");

    // Phoenix that flies across the sky every ~30 s
    SkinnedMesh phoenixMesh;
    bool havePhoenix = false;
    try {
        phoenixMesh = SkinnedMesh::load("assets/screen/phoenix/scene.gltf");
        phoenixMesh.loadTexture("assets/screen/phoenix/textures/MatI_Ride_FengHuang_01a_baseColor.png");
        havePhoenix = true;
    } catch (const std::exception &e) { std::cerr << "Phoenix load: " << e.what() << "\n"; }
    float phoenixTimer  = 27.f;     // first flyby ~3 s after the logo appears, then every 30 s
    bool  phoenixFlying = false;
    float phoenixProg   = 0.f;

    // ── living-diorama backdrop: a sand floor + swaying vegetation ──
    auto loadObjOpt = [](const char *p) -> std::unique_ptr<Mesh> {
        try { return std::make_unique<Mesh>(ObjLoader::load(p)); }
        catch (const std::exception &e) { std::cerr << "veg load " << p << ": " << e.what() << "\n"; return nullptr; }
    };
    std::unique_ptr<Mesh> palmMesh  = loadObjOpt("assets/models/palmtree_1.obj");
    std::unique_ptr<Mesh> grassMesh = loadObjOpt("assets/models/grass_large.obj");
    std::unique_ptr<Mesh> plantMesh = loadObjOpt("assets/models/plant_1.obj");
    // flat sand floor (interleaved pos+normal, normal up)
    Mesh sandMesh = [] {
        const float s = 22.f;
        std::vector<float> v = { -s,0,-s, 0,1,0,   s,0,-s, 0,1,0,
                                  s,0, s, 0,1,0,  -s,0, s, 0,1,0 };
        std::vector<unsigned int> idx = { 0,1,2, 0,2,3 };
        return Mesh(v, idx);
    }();
    struct Veg { Mesh *m; glm::vec3 pos; float scale, rotY; glm::vec3 col; };
    std::vector<Veg> veg;
    auto vegScale = [](Mesh *m, float h) { return m ? h / std::max(0.01f, m->boundingRadius()) : h; };
    if (palmMesh) {                              // palms framing the far shore
        veg.push_back({palmMesh.get(), {-7.f, 0.f, -10.f}, vegScale(palmMesh.get(), 4.0f),  0.3f, {0.30f,0.52f,0.26f}});
        veg.push_back({palmMesh.get(), { 7.5f,0.f, -11.f}, vegScale(palmMesh.get(), 4.4f), -0.5f, {0.28f,0.50f,0.24f}});
        veg.push_back({palmMesh.get(), {-2.f, 0.f, -13.f}, vegScale(palmMesh.get(), 3.6f),  1.1f, {0.26f,0.48f,0.22f}});
    }
    for (int i = 0; i < 9 && grassMesh; ++i) {   // grass tufts scattered on the sand
        float gx = frand(-9.f, 9.f), gz = frand(-12.f, -3.f);
        veg.push_back({grassMesh.get(), {gx, 0.f, gz}, vegScale(grassMesh.get(), frand(0.5f, 0.9f)),
                       frand(0.f, 6.28f), {0.40f, 0.62f, 0.28f}});
    }
    for (int i = 0; i < 5 && plantMesh; ++i) {   // a few leafy plants
        float px = frand(-8.f, 8.f), pz = frand(-11.f, -4.f);
