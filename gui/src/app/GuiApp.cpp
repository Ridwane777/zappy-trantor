// Map viewer — reads map.json, renders isometric Sadida world
// Hot-reloads the file whenever it changes on disk.
// Controls: drag to orbit, scroll to zoom, R to reset camera

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "AssetCache.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "app/SkyDayNight.hpp"
#include "app/Particle.hpp"
#include "app/Constellation.hpp"
#include "app/TreeMotes.hpp"
#include "app/IncantationFx.hpp"
#include "app/WorldRender.hpp"
#include "app/Character.hpp"
#include "app/CharacterRender.hpp"
#include "app/PlayerRender.hpp"
#include "render/ObjLoader.hpp"
#include "render/SkinnedMesh.hpp"
#include "render/TexturedMesh.hpp"
#include "menu/MenuScene.hpp"
#include "net/GuiClient.hpp"
#include "audio/AudioManager.hpp"
#include "hud/HudOverlay.hpp"
#include "app/GuiApp.hpp"
#include "GUIException.hpp"
#include "world/World.hpp"

#include <json.hpp>
using json = nlohmann::json;

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <sys/stat.h>

// ── application ─────────────────────────────────────────────────────────────
// GuiApp::run — owns the window, runs the start menu, then the live game loop
// (3D scene + HudOverlay). Constructed and invoked from main().
int GuiApp::run(int argc, char** argv)
{
    // ── window ────────────────────────────────────────────────────────────────
    sf::ContextSettings ctx;
    ctx.depthBits = 24; ctx.stencilBits = 8;
    ctx.majorVersion = 3; ctx.minorVersion = 3;
    ctx.attributeFlags = sf::ContextSettings::Default;
    ctx.antialiasingLevel = 4;

    sf::RenderWindow window(sf::VideoMode(1280, 720),
        "Zappy Map — Sadida", sf::Style::Default, ctx);
    window.setVerticalSyncEnabled(true);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW fail\n"; return 1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // ── startup menu (3D intro + connection config) ───────────────────────────
    const char* SYS_FONT = "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf";
    // Menu keeps the chunky display font; the in-game HUD/loading uses GAMEDAY,
    // whose numerals are legible (the menu font renders digits decoratively).
    // Fonts are streamed from the CDN into RAM; sf::Font keeps reading from that
    // buffer, so it must stay alive -> assetBytesKept (persistent store).
    const std::string& mfBytes = zappy::assetBytesKept("assets/fonts/GamersDemoBold.ttf");
    const std::string& hfBytes = zappy::assetBytesKept("assets/fonts/GAMEDAY.ttf");
    sf::Font menuFont;
    if (!(mfBytes.size() && menuFont.loadFromMemory(mfBytes.data(), mfBytes.size())) &&
        !menuFont.loadFromFile(SYS_FONT))
        std::cerr << "Warning: menu font not loaded\n";
    sf::Font hudFont;
    if (!(hfBytes.size() && hudFont.loadFromMemory(hfBytes.data(), hfBytes.size())) &&
        !hudFont.loadFromFile(SYS_FONT))
        std::cerr << "Warning: HUD font not loaded\n";

    // ── parallel asset prefetch (RAM, no disk) — kills the per-file freeze ────
    // Everything streams from the CDN; downloading sequentially on the main
    // thread froze the window. Instead we prefetch in parallel into the RAM
    // store: the intro set first (with a responsive bar), then the world set in
    // the background while the player is in the menu. Loaders then hit warm RAM.
    auto A = [](const std::string& s) { return "assets/" + s; };
    std::vector<std::string> introAssets = {
        A("screen/loading_bg.png"),
        A("screen/zappy_logo.obj"), A("screen/zappy_logo.png"),
        A("screen/water.png"), A("screen/settings_gear.png"),
        A("screen/phoenix/scene.gltf"), A("screen/phoenix/scene.bin"),
        A("screen/phoenix/textures/MatI_Ride_FengHuang_01a_baseColor.png"),
        A("screen/phoenix/textures/MatI_Ride_FengHuang_01a_emissive.png"),
        A("screen/phoenix/textures/MatI_Ride_FengHuang_01b_baseColor.png"),
        A("screen/phoenix/textures/MatI_Ride_FengHuang_01b_emissive.png"),
        A("models/palmtree_1.obj"), A("models/grass_large.obj"), A("models/plant_1.obj"),
    };
    // ALL audio is prefetched in the intro phase. The menu's audio.init() loads
    // the whole SFX set synchronously on the MAIN thread — if it streamed there,
    // the menu would freeze (window "not responding"). Warmed here, it's instant.
    for (const char* s : {"intro","menu_music","game_music",
                          "sparkle","bounce","shimmer_hold","burst","reveal","lensflare","twinkle",
                          "whoosh","click","levelup","hatch","incant","death","pickup",
                          "mew_cry","mew_float","mew_fly","mew_meow_a","mew_meow_b","mew_meow_c"})
        introAssets.push_back(A(std::string("audio/") + s + ".wav"));
    std::vector<std::string> gameAssets = {
        A("models/egg_tex.obj"), A("models/egg_tex.png"), A("models/cloud.obj"),
        A("characters/female/Meshy_AI_Huntress_of_the_Wild_biped_Animation_Idle_15_withSkin.glb"),
        A("characters/female/Meshy_AI_Huntress_of_the_Wild_biped_Animation_Walking_withSkin.glb"),
        A("characters/female/player_female.png"),
        A("characters/male/Meshy_AI_Ironhorn_Wanderer_biped_Animation_Idle_11_withSkin.glb"),
        A("characters/male/Meshy_AI_Ironhorn_Wanderer_biped_Animation_Walking_withSkin.glb"),
        A("characters/male/player_male.png"),
        A("characters/mew/scene.gltf"), A("characters/mew/scene.bin"),
        A("characters/mew/textures/blinn1_baseColor.jpeg"),
        A("models/moon/scene.gltf"), A("models/moon/scene.bin"),
        A("models/moon/textures/moon_baseColor.png"),
    };
    for (const char* g : {"linemate","deraumere","sibur","mendiane","phiras","thystame"}) {
        gameAssets.push_back(A(std::string("models/gem_") + g + "_lod.obj"));
        gameAssets.push_back(A(std::string("models/gem_") + g + ".png"));
    }
    for (const char* f : {"food1","food2","food3"}) {
        gameAssets.push_back(A(std::string("models/") + f + ".obj"));
        gameAssets.push_back(A(std::string("models/") + f + ".png"));
    }
    for (const char* d : {"woodsprite","nomad","nyxara","huntress"})
        for (const char* f : {"idle.glb","walk.glb","run.glb","turn_left.glb","turn_right.glb","player.png"})
            gameAssets.push_back(A(std::string("characters/") + d + "/" + f));
    for (const char* p : {"boat","cliff_rock","flower_1","flower_2","lily_pad","mapletree_1",
                          "normaltree_1","rock_1","rock_2","rock_3","tree_of_life"})
        gameAssets.push_back(A(std::string("models/") + p + ".obj"));   // audio prefetched in intro

    // Smooth, never-stuck loading bar: the displayed value eases toward the real
    // file-count fraction and gently creeps so it keeps moving even while a big
    // file downloads (real progress is per-file, so it would otherwise freeze).
    auto waitBar = [&](std::atomic<size_t>& done, size_t total, const char* label) {
        float shown = 0.f;
        while (done.load() < total) {
            float target = total ? (float)done.load() / total : 1.f;
            shown += (target - shown) * 0.20f + 0.0035f;       // ease + creep
            if (shown > target + 0.08f) shown = target + 0.08f; // don't run too far ahead
            if (shown > 0.985f)         shown = 0.985f;
            drawLoadingScreen(window, hudFont, shown, label);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        drawLoadingScreen(window, hudFont, 1.f, label);         // snap to 100%
    };

    {   // intro set: responsive loading bar (no freeze)
        std::atomic<size_t> d{0};
        std::thread t([&] { zappy::prefetch(introAssets, &d, 6); });
        waitBar(d, introAssets.size(), "Chargement de l'intro...");
        t.join();
    }
    // world set: prefetched in the background while the player is in the menu
    std::atomic<size_t> gameDone{0};
    std::thread gamePrefetch([&] { zappy::prefetch(gameAssets, &gameDone, 6); });

    MenuConfig menuCfg;
    // Standard launch:  ./zappy_gui -p port -h machine   (pre-fills the menu).
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-p" && i + 1 < argc)      menuCfg.port = std::atoi(argv[++i]);
        else if (a == "-h" && i + 1 < argc) menuCfg.host = argv[++i];
    }
    // Dev/CI hook: ZAPPY_AUTOSTART=host:port skips the menu and connects directly.
    bool proceed = true;
    if (const char* as = std::getenv("ZAPPY_AUTOSTART"); as && *as) {
        std::string a(as);
        auto colon = a.find(':');
        menuCfg.host = (colon == std::string::npos) ? a : a.substr(0, colon);
        if (colon != std::string::npos) menuCfg.port = std::atoi(a.c_str() + colon + 1);
        std::cout << "[menu] autostart -> " << menuCfg.host << ":" << menuCfg.port << "\n";
    } else {
        proceed = runMenu(window, menuFont, hudFont, menuCfg);
    }
    if (!proceed) { gamePrefetch.join(); return 0; }   // window closed / quit
    // make sure the world finished prefetching (if START was clicked early)
    waitBar(gameDone, gameAssets.size(), "Chargement du monde...");
    gamePrefetch.join();
    std::cout << "[menu] start -> server " << menuCfg.host << ":"
              << menuCfg.port << "\n";
    window.setTitle("Zappy Map — Sadida");
    // restore the GL state the game renderer expects after the menu
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Fullscreen quad for sky gradient (NDC coords, no depth)
    float skyQuadV[] = { -1,-1, 1,-1, 1,1, -1,1 };
    unsigned int skyQuadI[] = { 0,1,2, 0,2,3 };
    unsigned int skyVAO, skyVBO, skyEBO;
    glGenVertexArrays(1, &skyVAO);
    glGenBuffers(1, &skyVBO);
    glGenBuffers(1, &skyEBO);
    glBindVertexArray(skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyQuadV), skyQuadV, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyQuadI), skyQuadI, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ── shaders ───────────────────────────────────────────────────────────────
    Shader skyShader ("shaders/sky.vert",         "shaders/sky.frag");
    Shader toon      ("shaders/phong.vert",        "shaders/phong.frag");
    Shader outline   ("shaders/outline.vert",      "shaders/outline.frag");
    Shader skinShader("shaders/skinned.vert",      "shaders/phong.frag");
    Shader auraShader  ("shaders/skinned_aura.vert", "shaders/aura.frag");
    Shader incantShader("shaders/incant.vert",       "shaders/incant.frag");
    Shader starShader  ("shaders/star.vert",          "shaders/star.frag");
    Shader eggShader   ("shaders/logo.vert",          "shaders/logo.frag");

    // textured egg model (unhatched players)
    std::unique_ptr<TexturedMesh> eggMeshTex;
    try {
        eggMeshTex = std::make_unique<TexturedMesh>(
            TexturedMesh::load("assets/models/egg_tex.obj", "assets/models/egg_tex.png"));
    } catch (const std::exception& e) {
        std::cerr << "Egg texture load: " << e.what() << "\n";
    }

    // textured food variants (apple / grapes / mushroom)
    std::unique_ptr<TexturedMesh> foodMesh[3];
    const char* foodName[3] = { "food1", "food2", "food3" };
    for (int i = 0; i < 3; ++i) {
        try {
            foodMesh[i] = std::make_unique<TexturedMesh>(TexturedMesh::load(
                std::string("assets/models/") + foodName[i] + ".obj",
                std::string("assets/models/") + foodName[i] + ".png"));
        } catch (const std::exception& e) {
            std::cerr << "Food " << foodName[i] << " load: " << e.what() << "\n";
        }
    }

    // textured gemstones, one per stone type (indices 1..6, matching RES_NAMES)
    std::unique_ptr<TexturedMesh> gemTex[7];
    {
        const char* gemName[7] = { nullptr, "linemate", "deraumere", "sibur",
                                   "mendiane", "phiras", "thystame" };
        for (int i = 1; i < 7; ++i) {
            try {
                gemTex[i] = std::make_unique<TexturedMesh>(TexturedMesh::load(
                    std::string("assets/models/gem_") + gemName[i] + "_lod.obj",
                    std::string("assets/models/gem_") + gemName[i] + ".png"));
            } catch (const std::exception& e) {
                std::cerr << "Gem " << gemName[i] << " load: " << e.what() << "\n";
            }
        }
