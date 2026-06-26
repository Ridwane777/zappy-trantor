#include "menu/MenuScene.hpp"
#include "app/Fullscreen.hpp"
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

// Plain square ("carreau") maximize toggle — no texture, same look as a
// window manager's maximize/restore control.
struct SquareIconButton {
    sf::Vector2f pos;
    float size = 16.f;
    bool hovered = false;

    void setup(float px, float py, float sz) { pos = {px, py}; size = sz; }
    bool contains(float mx, float my) const
    {
        return mx >= pos.x - 4.f && my >= pos.y - 4.f &&
               mx <= pos.x + size + 4.f && my <= pos.y + size + 4.f;
    }
    void draw(sf::RenderWindow &w)
    {
        sf::RectangleShape sq(sf::Vector2f(size, size));
        sq.setPosition(pos);
        sq.setFillColor(sf::Color::Transparent);
        sq.setOutlineColor(hovered ? sf::Color(190, 220, 255, 230)
                                   : sf::Color(150, 170, 205, 200));
        sq.setOutlineThickness(1.6f);
        w.draw(sq);
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
        veg.push_back({plantMesh.get(), {px, 0.f, pz}, vegScale(plantMesh.get(), frand(0.8f, 1.3f)),
                       frand(0.f, 6.28f), {0.34f, 0.56f, 0.26f}});
    }

    auto envf = [](const char *n, float d) { const char *v = std::getenv(n); return v ? (float)std::atof(v) : d; };
    const float phxYaw = envf("ZAPPY_PHX_YAW", 0.f);    // phoenix facing (deg), 0 = along travel

    Mesh sphere = buildSphere(1.f, 20, 16);

    // Camera-facing quad used to billboard the glowing star (corners -1..1).
    unsigned int glowVAO = 0, glowVBO = 0;
    {
        const float quad[] = { -1.f,-1.f,  1.f,-1.f,  1.f,1.f,  -1.f,1.f };
        glGenVertexArrays(1, &glowVAO);
        glGenBuffers(1, &glowVBO);
        glBindVertexArray(glowVAO);
        glBindBuffer(GL_ARRAY_BUFFER, glowVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // Water plane for the logo's reflection (rippling, semi-transparent).
    Shader waterShader("shaders/water.vert", "shaders/water.frag");
    GLuint waterTex = 0;
    {
        sf::Image wi;
        std::string wb = zappy::assetBytes("assets/screen/water.png");
        if (!wb.empty() && wi.loadFromMemory(wb.data(), wb.size())) {
            glGenTextures(1, &waterTex);
            glBindTexture(GL_TEXTURE_2D, waterTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wi.getSize().x, wi.getSize().y,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, wi.getPixelsPtr());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // Logo: prefer the textured export; fall back to a plain mesh, then to a
    // procedural block, so the menu always renders something.
    std::unique_ptr<TexturedMesh> texLogo;
    std::unique_ptr<Mesh>         plainLogo;
    bool useTexLogo = false;
    try {
        texLogo = std::make_unique<TexturedMesh>(TexturedMesh::load(LOGO_PATH, LOGO_TEX));
        useTexLogo = texLogo->hasTexture();
        std::cout << "[menu] loaded textured logo: " << LOGO_PATH
                  << (useTexLogo ? " (+texture)\n" : " (no texture)\n");
    } catch (const std::exception &e) {
        std::cout << "[menu] textured logo failed (" << e.what()
                  << ") - using placeholder block\n";
        plainLogo = std::make_unique<Mesh>(buildPlaceholderLogo());
    }

    // Normalise logo to a consistent on-screen size regardless of source scale.
    float     logoScale  = 1.f;
    glm::vec3 logoCenter(0.f);
    {
        float r = useTexLogo ? texLogo->boundingRadius() : plainLogo->boundingRadius();
        logoScale  = (r > 1e-4f) ? 2.6f / r : 1.f;
        logoCenter = useTexLogo ? texLogo->centroid() : plainLogo->centroid();
    }

    // ── scene constants ──────────────────────────────────────────────────────
    const glm::vec3 LOGO_POS(0.f, 1.6f, 0.f);   // sits above the reflection plane y=0
    const float     LIGHT_START_Y = 4.6f;   // just inside the top of frame
    const float     IMPACT_Y      = LOGO_POS.y;

    // ── intro state machine ──────────────────────────────────────────────────
    enum class Phase { FALLING, FLOATING, REVEALING, READY };
    Phase phase = Phase::FALLING;
    float phaseT = 0.f;
    const float FALL_DUR   = 9.0f;   // blue light descends, brightening (slow)
    const float FLOAT_DUR  = 2.5f;   // bounces / floats at the centre
    const float REVEAL_DUR = 5.0f;   // flash + particles fade as the logo appears

    std::vector<Particle> particles;

    // drifting fireflies for the tropical-island ambience
    std::vector<Particle> fireflies;
    for (int i = 0; i < 16; ++i) {
        Particle p;
        p.pos    = glm::vec3(frand(-5.f, 5.f), frand(0.2f, 2.2f), frand(-3.f, 2.5f));
        p.vel    = glm::vec3(frand(-0.25f, 0.25f), frand(-0.12f, 0.18f), frand(-0.25f, 0.25f));
        p.size   = frand(0.006f, 0.013f);
        p.phase  = frand(0.f, 6.2832f);
        p.twFreq = frand(1.5f, 4.0f);
        p.color  = glm::mix(glm::vec3(1.0f, 0.85f, 0.4f),   // warm amber
                            glm::vec3(0.7f, 1.0f, 0.5f),    // green-gold
                            frand(0.f, 1.f));
        p.life = p.maxLife = 1.f;
        fireflies.push_back(p);
    }
    float lightY      = LIGHT_START_Y;
    float lightScale  = 0.05f;   // starts small
    float lightBright = 0.25f;   // starts dim
    float flashStr    = 0.f;     // point-light flash intensity (decays)
    float flash2D     = 0.f;     // full-screen flash overlay (decays)
    float logoReveal  = 0.f;     // 0..1
    float animT       = 0.f;     // accumulating time for pulse/flicker effects

    // ── UI ───────────────────────────────────────────────────────────────────
    Button startBtn;
    startBtn.setup(font, "START", 240.f, 60.f);

    // Settings gear icon in the top-left corner.
    sf::Texture gearTex;
    IconButton settingsBtn;
    std::string gb = zappy::assetBytes("assets/screen/settings_gear.png");
    if (!gb.empty() && gearTex.loadFromMemory(gb.data(), gb.size())) {
        gearTex.setSmooth(true);
        settingsBtn.setup(gearTex, 24.f, 24.f, 48.f);
    }

    // Fullscreen toggle, just below the gear icon.
    SquareIconButton fullscreenBtn;
    fullscreenBtn.setup(24.f, 84.f, 16.f);
    bool fullscreen = false;

    bool showParams = false;
    int  focusField = -1;          // 0 = host, 1 = port
    // orbit camera around the fixed logo (3rd-person style)
    float camYaw   = 0.f;
    float camPitch = 0.18f;
    // the scene (sky, sand, vegetation, water) stays FIXED; dragging spins only the
    // logo on the spot, so the backdrop feels like a living diorama you look into.
    float logoYaw   = 0.f;
    float logoPitch = 0.f;
    float camDist  = 9.0f;
    bool  orbiting = false;
    sf::Vector2i lastDrag;
    std::string hostStr = out.host;
    std::string portStr = std::to_string(out.port);

    sf::Clock clock;

    auto spawnExplosion = [&](const glm::vec3 &c) {
        particles.clear();
        int n = 280;
        for (int i = 0; i < n; ++i) {
            glm::vec3 dir(frand(-1, 1), frand(-1, 1), frand(-1, 1));
            float len = glm::length(dir);
            if (len < 1e-3f) dir = glm::vec3(0, 1, 0); else dir /= len;
            float speed = frand(1.5f, 6.0f);
            Particle p;
            p.pos = c;
            p.vel = dir * speed;
            p.maxLife = frand(0.8f, 1.8f);
            p.life = p.maxLife;
            p.size = frand(0.015f, 0.06f);             // small motes of light
            p.color = glm::mix(glm::vec3(0.45f, 0.70f, 1.0f),   // blue
                               glm::vec3(0.90f, 0.95f, 1.0f),   // near-white
                               frand(0.f, 1.f));
            p.phase  = frand(0.f, 6.2832f);            // random twinkle phase
            p.twFreq = frand(10.f, 26.f);              // irregular flicker rates
            particles.push_back(p);
        }
    };

    // ── intro audio: music bed + per-phase cues (missing files are silent) ──────
    AudioManager audio;
    audio.init("assets/audio");
    audio.playMusic("intro", false, 32.f);     // quiet bed so the star effects carry
    audio.playLoop("sparkle", 0.f);            // starts silent on window open, swells with the fall
    int lastBounce = -1;                        // tracks floor contacts during FLOATING

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f;            // clamp big hitches (asset load)
        phaseT += dt;
        animT  += dt;

        // ── events ────────────────────────────────────────────────────────────
        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed)
                return false;
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Escape) {
                if (showParams) showParams = false;
                else return false;
            }
            // Enter starts the game once the intro has finished (same as START).
            if (ev.type == sf::Event::KeyPressed && phase == Phase::READY && !showParams &&
                (ev.key.code == sf::Keyboard::Return || ev.key.code == sf::Keyboard::Enter)) {
                out.host = hostStr.empty() ? "127.0.0.1" : hostStr;
                out.port = portStr.empty() ? 4242 : std::atoi(portStr.c_str());
                return true;
            }
            if (ev.type == sf::Event::Resized) {
                glViewport(0, 0, ev.size.width, ev.size.height);
                // SFML's 2D view doesn't auto-follow a WM-driven resize
                // (maximize toggle); without this the gear/fullscreen icons
                // and START button drift off the new window bounds.
                window.setView(sf::View(sf::FloatRect(0.f, 0.f,
                    (float)ev.size.width, (float)ev.size.height)));
            }

            sf::Vector2u ws = window.getSize();

            if (ev.type == sf::Event::MouseButtonPressed)
                audio.play("click", 70.f);                 // son à chaque clic

            if (ev.type == sf::Event::MouseButtonPressed &&
                ev.mouseButton.button == sf::Mouse::Left) {
                float mx = float(ev.mouseButton.x), my = float(ev.mouseButton.y);
                if (showParams) {
                    // field hit-tests (positions mirror the draw code below)
                    float fx = ws.x / 2.f - 150.f, fw = 300.f, fh = 40.f;
                    sf::FloatRect hostR(fx, ws.y / 2.f - 40.f, fw, fh);
                    sf::FloatRect portR(fx, ws.y / 2.f + 30.f, fw, fh);
                    if (hostR.contains(mx, my))      focusField = 0;
                    else if (portR.contains(mx, my)) focusField = 1;
                    else                             focusField = -1;
                    // "Back / Done" button
                    sf::FloatRect backR(ws.x / 2.f - 90.f, ws.y / 2.f + 100.f, 180.f, 48.f);
                    if (backR.contains(mx, my)) {
                        showParams = false;
                        focusField = -1;
                    }
                } else if (phase == Phase::READY && startBtn.contains(mx, my)) {
                    out.host = hostStr.empty() ? "127.0.0.1" : hostStr;
                    out.port = portStr.empty() ? 4242 : std::atoi(portStr.c_str());
                    return true;
                } else if (phase == Phase::READY && settingsBtn.valid &&
                           settingsBtn.contains(mx, my)) {
                    showParams = true;
                    focusField = 0;
                } else if (fullscreenBtn.contains(mx, my)) {
                    fullscreen = !fullscreen;
                    setWindowFullscreen(window.getSystemHandle(), fullscreen);
                } else {
                    // click anywhere else starts orbiting the camera around the logo
                    orbiting = true;
                    lastDrag = {ev.mouseButton.x, ev.mouseButton.y};
                }
            }
            if (ev.type == sf::Event::MouseButtonReleased &&
                ev.mouseButton.button == sf::Mouse::Left)
                orbiting = false;

            if (ev.type == sf::Event::MouseMoved && orbiting) {
                // spin the logo in place; the camera/scene never moves
                logoYaw   += (ev.mouseMove.x - lastDrag.x) * 0.012f;
                logoPitch += (ev.mouseMove.y - lastDrag.y) * 0.008f;
                logoPitch  = glm::clamp(logoPitch, -0.5f, 0.5f);
                lastDrag = {ev.mouseMove.x, ev.mouseMove.y};
            }
            if (ev.type == sf::Event::MouseWheelScrolled && !showParams)
                camDist = glm::clamp(camDist - ev.mouseWheelScroll.delta * 0.8f, 4.f, 20.f);

            if (ev.type == sf::Event::TextEntered && showParams && focusField >= 0) {
                std::string &f = (focusField == 0) ? hostStr : portStr;
                char c = static_cast<char>(ev.text.unicode);
                if (ev.text.unicode == 8) {                 // backspace
                    if (!f.empty()) f.pop_back();
                } else if (ev.text.unicode >= 32 && ev.text.unicode < 127) {
                    if (focusField == 1) {                  // port: digits only
                        if (c >= '0' && c <= '9' && f.size() < 5) f += c;
                    } else if (f.size() < 64) {
                        f += c;
                    }
                }
            }
        }

        // ── update intro ──────────────────────────────────────────────────────
        if (phase == Phase::FALLING) {
            float t = std::min(phaseT / FALL_DUR, 1.f);
            audio.setLoopVolume(glm::mix(0.f, 100.f, t));    // 0 at window open → full at centre
            float e = t * t;                                 // ease-in (accelerate)
            lightY = glm::mix(LIGHT_START_Y, IMPACT_Y, e);
            lightScale = glm::mix(0.05f, 0.20f, t);          // grows as it falls
            // brightens (accelerating) with an uncanny flicker
            float flicker = 0.86f + 0.14f * std::sin(animT * 34.f);
            lightBright = glm::mix(0.25f, 1.6f, t * t) * flicker;
            if (t >= 1.f) {
                phase = Phase::FLOATING; phaseT = 0.f;
                audio.stopLoop();                            // star reached centre → sparkle ends
                audio.play("shimmer_hold", 55.f);            // soft glow bed under the bounces
            }
        } else if (phase == Phase::FLOATING) {
            // damped bounce on an invisible floor at the impact point
            float decay  = 1.f - std::min(phaseT / FLOAT_DUR, 1.f);
            float bounce = std::fabs(std::sin(phaseT * 5.5f)) * 0.6f * decay;
            lightY = IMPACT_Y + bounce;
            // each time the light returns to the floor (|sin| hits 0) -> a bounce sfx
            int bIdx = (int)(phaseT * 5.5f / 3.14159265f);
            if (bIdx != lastBounce) {
                lastBounce = bIdx;
                audio.play("bounce", 55.f + 45.f * decay);   // softens as it settles
            }
            lightScale  = 0.20f + 0.02f * std::sin(animT * 9.f);
            lightBright = 1.6f + 0.35f * std::sin(animT * 13.f);   // bright, uneasy pulse
            if (phaseT >= FLOAT_DUR) {
                phase = Phase::REVEALING; phaseT = 0.f;
                spawnExplosion(LOGO_POS);                    // flash + motes of light
                flashStr = 1.f;
                flash2D  = 1.f;
                audio.play("burst", 100.f);                  // light explodes into motes
                audio.play("lensflare", 100.f);              // logo sets in with a lens-flare shimmer
            }
        } else if (phase == Phase::REVEALING) {
            // logo eases in over the first ~55% while the flash/motes fade out
            logoReveal = std::min(phaseT / (REVEAL_DUR * 0.55f), 1.f);
            if (phaseT >= REVEAL_DUR) {
                phase = Phase::READY; phaseT = 0.f;
                audio.playMusic("menu_music", true, 40.f);   // calm loop while in the menu
            }
        } else {
            logoReveal = 1.f;
        }

        // particles + flash decay
        for (auto &p : particles) {
            p.vel.y -= 2.0f * dt;                            // gentle gravity (floaty)
            p.vel   *= (1.f - 0.6f * dt);                    // air drag
            p.pos += p.vel * dt;
            p.life -= dt;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle &p) { return p.life <= 0.f; }), particles.end());
        flashStr = std::max(0.f, flashStr - dt * 2.0f);
        flash2D  = std::max(0.f, flash2D  - dt * 0.667f);    // flash fade (~1.5s)

        // ── camera (orbits the fixed logo; drag to look around it) ────────────
        sf::Vector2u ws = window.getSize();
        float aspect = (ws.y > 0) ? float(ws.x) / float(ws.y) : 1.7f;
        glm::vec3 target(0.f, 1.5f, 0.f);                 // orbit pivot = the logo
        glm::vec3 camPos = target + camDist * glm::vec3(
            std::cos(camPitch) * std::sin(camYaw),
            std::sin(camPitch),
            std::cos(camPitch) * std::cos(camYaw));
        glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(42.f), aspect, 0.1f, 200.f);

        // ── render 3D ─────────────────────────────────────────────────────────
        glClearColor(0.015f, 0.02f, 0.035f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // tropical sunset sky — only after the explosion reveals the scene
        glm::vec3 sunDir = glm::normalize(glm::vec3(0.35f, 0.16f, -0.92f));
        if (phase == Phase::REVEALING || phase == Phase::READY) {
            glm::mat4 invVP  = glm::inverse(proj * view);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            skyShader.use();
            skyShader.setVec3 ("uSkyTop",      0.16f, 0.26f, 0.55f);
            skyShader.setVec3 ("uSkyHorizon",  0.98f, 0.52f, 0.30f);
            skyShader.setMat4 ("uInvViewProj", glm::value_ptr(invVP));
            skyShader.setVec3 ("uCamPos",      camPos.x, camPos.y, camPos.z);
            skyShader.setVec3 ("uSunDir",      sunDir.x, sunDir.y, sunDir.z);
            skyShader.setVec3 ("uMoonDir",     0.f, -1.f, 0.f);
            skyShader.setFloat("uSunStrength",  1.f);
            skyShader.setFloat("uMoonStrength", 0.f);
            glBindVertexArray(glowVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // ── sand floor + swaying vegetation: fixed living backdrop ──
        if (phase == Phase::REVEALING || phase == Phase::READY) {
            setPhongDefaults(phong);
            phong.use();
            phong.setMat4 ("uView",  glm::value_ptr(view));
            phong.setMat4 ("uProj",  glm::value_ptr(proj));
            phong.setVec3 ("uCamPos", camPos.x, camPos.y, camPos.z);
            phong.setVec3 ("uDirLightDir",   -0.4f, -0.8f, -0.5f);
            phong.setVec3 ("uDirLightColor",  1.0f, 0.85f, 0.6f);   // warm sunset light
            phong.setVec3 ("uPointLight0Color", 0.f, 0.f, 0.f);
            phong.setVec3 ("uPointLight1Color", 0.f, 0.f, 0.f);
            phong.setFloat("uAmbientStrength", 0.5f);
            phong.setFloat("uTime", animT);
            glDisable(GL_CULL_FACE);                                // veg double-sided

            // sand floor — soft-faded disc that melts into the water/horizon
            sandShader.use();
            sandShader.setMat4 ("uView", glm::value_ptr(view));
            sandShader.setMat4 ("uProj", glm::value_ptr(proj));
            sandShader.setFloat("uSize", 30.f);
            sandShader.setFloat("uY",   -0.05f);
            sandShader.setVec3 ("uColor",    0.90f, 0.82f, 0.58f);
            sandShader.setVec3 ("uColorFar", 0.82f, 0.74f, 0.66f);  // hazier toward the horizon
            sandShader.setFloat("uAlpha", 1.0f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glBindVertexArray(glowVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            // swaying vegetation
            phong.use();
            phong.setBool("uSway", true);
            for (auto &vg : veg) {
                glm::mat4 M = glm::translate(glm::mat4(1.f), vg.pos);
                M = glm::rotate(M, vg.rotY, glm::vec3(0.f, 1.f, 0.f));
                M = glm::scale(M, glm::vec3(vg.scale));
                glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
                phong.setVec3("uColor", vg.col.r, vg.col.g, vg.col.b);
                phong.setMat4("uModel", glm::value_ptr(M));
                phong.setMat3("uNormalMat", glm::value_ptr(N3));
                vg.m->draw();
            }
            phong.setBool("uSway", false);

            // toon outline around the vegetation (expanded back faces = silhouette)
            outline.use();
            outline.setMat4 ("uView", glm::value_ptr(view));
            outline.setMat4 ("uProj", glm::value_ptr(proj));
            outline.setFloat("uTime", animT);
            outline.setBool ("uSway", true);
            outline.setFloat("uOutlineWidth", 0.005f);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            for (auto &vg : veg) {
                glm::mat4 M = glm::translate(glm::mat4(1.f), vg.pos);
                M = glm::rotate(M, vg.rotY, glm::vec3(0.f, 1.f, 0.f));
                M = glm::scale(M, glm::vec3(vg.scale));
                glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
                outline.setMat4("uModel", glm::value_ptr(M));
                outline.setMat3("uNormalMat", glm::value_ptr(N3));
                vg.m->draw();
            }
            glCullFace(GL_BACK);
        }

        // ── phoenix flyby across the sky every ~30 s ──
        if (havePhoenix && phase == Phase::READY) {
            if (!phoenixFlying) {
                phoenixTimer += dt;
                if (phoenixTimer >= 30.f) { phoenixFlying = true; phoenixProg = 0.f; phoenixTimer = 0.f; }
            } else {
                phoenixProg += dt / 8.f;                 // ~8 s to cross
                if (phoenixProg >= 1.f) phoenixFlying = false;
            }
            if (phoenixFlying) {
                glm::mat4 pBones[MAX_BONES];
                phoenixMesh.computeBones(0, animT, pBones, true, false);   // "Take 001"
                float px = glm::mix(-16.f, 16.f, phoenixProg);
                float py = 3.0f + std::sin(phoenixProg * 3.14159f) * 0.8f; // gentle arc
                float pz = -2.f;                                          // in front of the vegetation
                glm::mat4 M = glm::translate(glm::mat4(1.f), glm::vec3(px, py, pz));
                M = glm::rotate(M, glm::radians(phxYaw), glm::vec3(0, 1, 0));  // face travel
                M = glm::scale(M, glm::vec3(0.00225f));  // half size, in the foreground
                glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));
                setPhongDefaults(skinShader);
                skinShader.use();
                skinShader.setMat4 ("uView",  glm::value_ptr(view));
                skinShader.setMat4 ("uProj",  glm::value_ptr(proj));
                skinShader.setVec3 ("uCamPos", camPos.x, camPos.y, camPos.z);
                skinShader.setVec3 ("uDirLightDir",   -0.4f, -0.6f, -0.7f);
                skinShader.setVec3 ("uDirLightColor",  1.0f, 0.72f, 0.5f);
                skinShader.setVec3 ("uPointLight0Color", 0.f, 0.f, 0.f);
                skinShader.setVec3 ("uPointLight1Color", 0.f, 0.f, 0.f);
                skinShader.setFloat("uAmbientStrength", 0.7f);
                skinShader.setBool ("uUseTexture", true);
                skinShader.setInt  ("uTexture", 0);
                skinShader.setMat4Array("uBones[0]", MAX_BONES, glm::value_ptr(pBones[0]));
                skinShader.setMat4 ("uModel",     glm::value_ptr(M));
                skinShader.setMat3 ("uNormalMat", glm::value_ptr(N3));
                glDisable(GL_CULL_FACE);
                phoenixMesh.draw();
                glEnable(GL_CULL_FACE);
            }
        }

        setPhongDefaults(phong);
        phong.use();
        phong.setMat4("uView", glm::value_ptr(view));
        phong.setMat4("uProj", glm::value_ptr(proj));
        phong.setVec3("uCamPos", camPos.x, camPos.y, camPos.z);
        phong.setVec3("uDirLightDir", -0.4f, -1.0f, -0.5f);
        phong.setVec3("uDirLightColor", 0.7f, 0.75f, 0.95f);
        // point light 0 = explosion flash; point light 1 unused
        phong.setVec3("uPointLight0Pos", LOGO_POS.x, LOGO_POS.y, LOGO_POS.z);
        phong.setVec3("uPointLight0Color", flashStr * 1.6f, flashStr * 2.2f, flashStr * 3.f);
        phong.setVec3("uPointLight1Pos", 0.f, 10.f, 0.f);
        phong.setVec3("uPointLight1Color", 0.f, 0.f, 0.f);

        auto drawMesh = [&](Mesh &m, const glm::mat4 &model, glm::vec3 color,
                            bool emissive, float alpha) {
            glm::mat3 nrm = glm::mat3(glm::transpose(glm::inverse(model)));
            phong.use();
            phong.setMat4("uModel", glm::value_ptr(model));
            phong.setMat3("uNormalMat", glm::value_ptr(nrm));
            phong.setVec3("uColor", color.r, color.g, color.b);
            phong.setBool("uEmissive", emissive);
            phong.setFloat("uAlpha", alpha);
            m.draw();
        };

        // falling light
        if (phase == Phase::FALLING || phase == Phase::FLOATING) {
            starShader.use();
            starShader.setMat4("uView", glm::value_ptr(view));
            starShader.setMat4("uProj", glm::value_ptr(proj));
            starShader.setVec3("uCenter", 0.f, lightY, 0.f);
            starShader.setFloat("uSize", lightScale * 2.5f);  // halo bigger than core
            starShader.setVec3("uColor", 0.30f, 0.55f, 1.0f); // blue glow
            starShader.setFloat("uIntensity", lightBright);

            glDepthMask(GL_FALSE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);                       // additive
            glBindVertexArray(glowVAO);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
        }

        // explosion motes — each a tiny glowing star, twinkling uncannily
        if (!particles.empty()) {
            starShader.use();
            starShader.setMat4("uView", glm::value_ptr(view));
            starShader.setMat4("uProj", glm::value_ptr(proj));
            glDepthMask(GL_FALSE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);                 // additive
            glBindVertexArray(glowVAO);
            for (auto &p : particles) {
                float k = p.life / p.maxLife;                       // 1 -> 0 fade
                float twinkle = 0.55f + 0.45f * std::sin(animT * p.twFreq + p.phase);
                starShader.setVec3 ("uCenter", p.pos.x, p.pos.y, p.pos.z);
                starShader.setFloat("uSize",   p.size * 6.0f);      // small star
                starShader.setVec3 ("uColor",  p.color.r, p.color.g, p.color.b);
                starShader.setFloat("uIntensity", (0.5f + 0.9f * k) * twinkle);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
            glBindVertexArray(0);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
        }

        // drifting fireflies (tropical ambience), once the scene has settled
        if (phase == Phase::READY) {
            for (auto &p : fireflies) {
                p.vel += glm::vec3(frand(-0.4f, 0.4f), frand(-0.3f, 0.3f), frand(-0.4f, 0.4f)) * dt;
                p.vel  = glm::clamp(p.vel, glm::vec3(-0.5f), glm::vec3(0.5f));
                p.pos += p.vel * dt;
                if (p.pos.x < -5.f) p.pos.x = 5.f;  if (p.pos.x >  5.f) p.pos.x = -5.f;
                if (p.pos.z < -3.f) p.pos.z = 2.5f; if (p.pos.z >  2.5f) p.pos.z = -3.f;
                if (p.pos.y <  0.2f){ p.pos.y = 0.2f; p.vel.y = std::abs(p.vel.y); }
                if (p.pos.y >  2.5f){ p.pos.y = 2.5f; p.vel.y = -std::abs(p.vel.y); }
            }
            starShader.use();
            starShader.setMat4("uView", glm::value_ptr(view));
            starShader.setMat4("uProj", glm::value_ptr(proj));
            glDepthMask(GL_FALSE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBindVertexArray(glowVAO);
            for (auto &p : fireflies) {
                float s  = 0.5f + 0.5f * std::sin(animT * p.twFreq + p.phase);
                float tw = 0.15f + 0.85f * s * s * s;        // firefly pulse (mostly off, soft blinks)
                starShader.setVec3 ("uCenter", p.pos.x, p.pos.y, p.pos.z);
                starShader.setFloat("uSize",   p.size * 2.2f);
                starShader.setVec3 ("uColor",  p.color.r, p.color.g, p.color.b);
                starShader.setFloat("uIntensity", 0.5f * tw);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
            glBindVertexArray(0);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
        }

        // logo + reflection (once revealing)
        if (phase == Phase::REVEALING || phase == Phase::READY) {
            float bob  = std::sin(animT * 1.2f) * 0.06f;
            float rise = (1.f - logoReveal) * 0.8f;     // settle down into place

            auto logoModel = [&](bool mirrored) {
                glm::mat4 m(1.f);
                if (mirrored)                            // reflect across plane y=0
                    m = glm::scale(glm::mat4(1.f), glm::vec3(1.f, -1.f, 1.f));
                m = glm::translate(m, LOGO_POS + glm::vec3(0.f, bob - rise, 0.f));
                m = glm::rotate(m, logoYaw,   glm::vec3(0.f, 1.f, 0.f));   // drag spins the logo
                m = glm::rotate(m, logoPitch, glm::vec3(1.f, 0.f, 0.f));
                m = glm::scale(m, glm::vec3(logoScale));
                m = glm::translate(m, -logoCenter);
                return m;
            };

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);                      // draw logo double-sided

            if (useTexLogo) {
                logoShader.use();
                logoShader.setMat4("uView", glm::value_ptr(view));
                logoShader.setMat4("uProj", glm::value_ptr(proj));
                logoShader.setInt("uTex", 0);
                logoShader.setVec3("uDirLightDir", -0.4f, -1.0f, -0.5f);
                logoShader.setVec3("uDirLightColor", 0.85f, 0.85f, 0.95f);
                logoShader.setVec3("uPointPos", LOGO_POS.x, LOGO_POS.y, LOGO_POS.z);
                logoShader.setVec3("uPointColor", flashStr * 1.4f, flashStr * 1.9f, flashStr * 2.6f);
                logoShader.setFloat("uAmbient", 0.45f);

                auto drawLogo = [&](const glm::mat4 &model, float alpha, float bright) {
                    glm::mat3 nrm = glm::mat3(glm::transpose(glm::inverse(model)));
                    logoShader.setMat4("uModel", glm::value_ptr(model));
                    logoShader.setMat3("uNormalMat", glm::value_ptr(nrm));
                    logoShader.setFloat("uAlpha", alpha);
                    logoShader.setFloat("uBright", bright);
                    texLogo->draw();
                };
                drawLogo(logoModel(true),  0.30f * logoReveal, 0.45f);   // reflection
                drawLogo(logoModel(false), logoReveal,         1.0f);    // logo
            } else {
                glm::vec3 col(0.55f, 0.62f, 0.85f);
                drawMesh(*plainLogo, logoModel(true),  col * 0.5f, false, 0.28f * logoReveal);
                drawMesh(*plainLogo, logoModel(false), col,        false, logoReveal);
            }

            // water surface at y=0 — the logo ripples in its reflection below
            if (waterTex) {
                waterShader.use();
                waterShader.setMat4 ("uView", glm::value_ptr(view));
                waterShader.setMat4 ("uProj", glm::value_ptr(proj));
                waterShader.setFloat("uTime",  animT);
                waterShader.setFloat("uSize",  7.0f);
                waterShader.setFloat("uAlpha", 0.5f * logoReveal);
                glm::vec2 sunXZ = glm::normalize(glm::vec2(sunDir.x, sunDir.z));
                waterShader.setVec2 ("uSunXZ",  sunXZ.x, sunXZ.y);
                waterShader.setVec3 ("uSunCol", 1.0f, 0.65f, 0.30f);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, waterTex);
                waterShader.setInt("uTex", 0);
                glDepthMask(GL_FALSE);
                glBindVertexArray(glowVAO);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                glBindVertexArray(0);
                glDepthMask(GL_TRUE);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glDisable(GL_BLEND);
        }

        // ── render 2D overlay ─────────────────────────────────────────────────
        window.resetGLStates();

        // full-screen white-out at the burst that fades over ~4s, revealing the
        // logo as it clears. The sqrt curve holds it bright (full cover) first.
        if (flash2D > 0.001f) {
            sf::RectangleShape flash(sf::Vector2f((float)ws.x, (float)ws.y));
            float f = std::sqrt(std::min(1.f, flash2D));
            sf::Uint8 a = (sf::Uint8)(f * 255.f);
            flash.setFillColor(sf::Color(235, 243, 255, a));
            window.draw(flash);
        }

        if (phase == Phase::READY && !showParams) {
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            // centered START button
            startBtn.place(ws.x / 2.f, ws.y - 90.f);
            startBtn.hovered = startBtn.contains((float)mp.x, (float)mp.y);
            startBtn.draw(window);
            // settings gear in the top-left corner
            if (settingsBtn.valid) {
                settingsBtn.hovered = settingsBtn.contains((float)mp.x, (float)mp.y);
                settingsBtn.draw(window);
            }
            // maximize toggle, just below the gear
            fullscreenBtn.hovered = fullscreenBtn.contains((float)mp.x, (float)mp.y);
            fullscreenBtn.draw(window);
        }

        if (showParams) {
            // dim panel
            sf::RectangleShape panel({420.f, 280.f});
            panel.setPosition(ws.x / 2.f - 210.f, ws.y / 2.f - 130.f);
            panel.setFillColor(sf::Color(18, 22, 34, 235));
            panel.setOutlineThickness(2.f);
            panel.setOutlineColor(sf::Color(90, 110, 150));
            window.draw(panel);

            auto drawField = [&](const std::string &caption, const std::string &val,
                                 float y, bool focused) {
                sf::Text cap(caption, uiFont, 18);
                cap.setFillColor(sf::Color(150, 165, 200));
                cap.setPosition(ws.x / 2.f - 150.f, y - 26.f);
                window.draw(cap);
                sf::RectangleShape box({300.f, 40.f});
                box.setPosition(ws.x / 2.f - 150.f, y);
                box.setFillColor(sf::Color(35, 42, 60));
                box.setOutlineThickness(2.f);
                box.setOutlineColor(focused ? sf::Color(150, 200, 255)
                                            : sf::Color(70, 85, 120));
                window.draw(box);
                sf::Text t(val + (focused ? "_" : ""), uiFont, 22);
                t.setFillColor(sf::Color::White);
                t.setPosition(ws.x / 2.f - 142.f, y + 6.f);
                window.draw(t);
            };

            sf::Text title("PARAMETERS", uiFont, 26);
            title.setFillColor(sf::Color(210, 220, 245));
            sf::FloatRect tb = title.getLocalBounds();
            title.setPosition(ws.x / 2.f - tb.width / 2.f, ws.y / 2.f - 120.f);
            window.draw(title);

            drawField("Server host", hostStr, ws.y / 2.f - 40.f, focusField == 0);
            drawField("Server port", portStr, ws.y / 2.f + 30.f, focusField == 1);

            sf::RectangleShape back({180.f, 48.f});
            back.setPosition(ws.x / 2.f - 90.f, ws.y / 2.f + 100.f);
            sf::Vector2i mp = sf::Mouse::getPosition(window);
            bool bh = back.getGlobalBounds().contains((float)mp.x, (float)mp.y);
            back.setFillColor(bh ? sf::Color(70, 90, 140) : sf::Color(30, 38, 58));
            back.setOutlineThickness(2.f);
            back.setOutlineColor(sf::Color(120, 150, 200));
            window.draw(back);
            sf::Text bt("DONE", uiFont, 22);
            sf::FloatRect bb = bt.getLocalBounds();
            bt.setFillColor(sf::Color::White);
            bt.setPosition(ws.x / 2.f - bb.width / 2.f, ws.y / 2.f + 112.f);
            window.draw(bt);
        }

        // restore GL state for our next 3D frame
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        window.display();
    }
    return false;
}

void drawLoadingScreen(sf::RenderWindow &window, sf::Font &font,
                       float progress, const std::string &status)
{
    // keep the window responsive while we block on asset loading
    sf::Event ev;
    while (window.pollEvent(ev)) {
        if (ev.type == sf::Event::Closed)
            window.close();
        // Without this, a WM-driven resize (maximize toggle) leaves the 2D
        // view mapped to the old size, so the progress bar/text render off
        // the visible area instead of tracking the new window bounds.
        if (ev.type == sf::Event::Resized) {
            glViewport(0, 0, ev.size.width, ev.size.height);
            window.setView(sf::View(sf::FloatRect(0.f, 0.f,
                (float)ev.size.width, (float)ev.size.height)));
        }
    }

    if (progress < 0.f) progress = 0.f;
    if (progress > 1.f) progress = 1.f;

    sf::Vector2u ws = window.getSize();
    float W = (float)ws.x, H = (float)ws.y;

    glClearColor(0.015f, 0.02f, 0.035f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    window.resetGLStates();

    // ── fullscreen background art (streamed once from the CDN, cover-fit) ──
    static sf::Texture bgTex;
    static bool bgLoaded = false;
    if (!bgLoaded) {   // NON-BLOCKING: picks up the image only once the prefetch has
        std::string b = zappy::assetBytesIfCached("assets/screen/loading_bg.png");
        if (!b.empty() && bgTex.loadFromMemory(b.data(), b.size())) {
            bgTex.setSmooth(true);
            bgLoaded = true;
        }
    }
    if (bgLoaded) {
        sf::Vector2u ts = bgTex.getSize();
        float s = std::max(W / (float)ts.x, H / (float)ts.y);   // cover
        sf::Sprite bg(bgTex);
        bg.setScale(s, s);
        bg.setPosition((W - ts.x * s) / 2.f, (H - ts.y * s) / 2.f);
        window.draw(bg);
    }

    // ── bottom info band ──
    float bandH = std::max(64.f, H * 0.12f);
    sf::RectangleShape band({W, bandH});
    band.setPosition(0.f, H - bandH);
    band.setFillColor(sf::Color(8, 7, 16, 205));
    window.draw(band);

    // ── loading bar: white-outlined rectangle filled with solid white ──
    float margin = W * 0.045f;
    float barW = W - 2.f * margin;
    float barH = 16.f;
    float bx = margin;
    float by = H - bandH - 30.f;

    sf::Text lbl("Loading", font, 20);
    lbl.setFillColor(sf::Color::White);
    lbl.setPosition(bx, by - 30.f);
    window.draw(lbl);

    sf::Text pct(std::to_string(int(progress * 100.f + 0.5f)) + "%", font, 20);
    sf::FloatRect pr = pct.getLocalBounds();
    pct.setFillColor(sf::Color::White);
    pct.setPosition(bx + barW - pr.width - pr.left, by - 30.f);
    window.draw(pct);

    sf::RectangleShape track({barW, barH});            // empty white frame
    track.setPosition(bx, by);
    track.setFillColor(sf::Color(255, 255, 255, 28));
    track.setOutlineThickness(2.f);
    track.setOutlineColor(sf::Color::White);
    window.draw(track);

    float fillW = barW * progress;                     // white fill
    if (fillW > 1.f) {
        sf::RectangleShape fill({fillW, barH});
        fill.setPosition(bx, by);
        fill.setFillColor(sf::Color::White);
        window.draw(fill);
    }

    // ── tip / status inside the band ──
    sf::Text brand("ZAPPY", font, 22);
    brand.setStyle(sf::Text::Bold);
    brand.setLetterSpacing(2.f);
    brand.setFillColor(sf::Color(255, 206, 120));
    brand.setPosition(margin, H - bandH + 12.f);
    window.draw(brand);

    sf::Text tip(status, font, 17);
    tip.setFillColor(sf::Color(214, 214, 228));
    tip.setPosition(margin, H - bandH + 42.f);
    window.draw(tip);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    window.display();
}

// Displayed bar value, persisted across checkpoints so transitions are smooth.
static float g_loadShown = 0.f;

void advanceLoadingTo(sf::RenderWindow &window, sf::Font &font,
                      float target, const std::string &status, float duration)
{
    if (target < g_loadShown)        // a new loading sequence — restart from 0
        g_loadShown = 0.f;
    if (target > 1.f) target = 1.f;

    float start = g_loadShown;
    sf::Clock clock;
    while (window.isOpen()) {
        float t = (duration > 0.f) ? clock.getElapsedTime().asSeconds() / duration : 1.f;
        if (t >= 1.f) t = 1.f;
        float e = t * t * (3.f - 2.f * t);          // smoothstep ease
        g_loadShown = start + (target - start) * e;
        drawLoadingScreen(window, font, g_loadShown, status);
        if (t >= 1.f) break;
    }
    g_loadShown = target;
}
