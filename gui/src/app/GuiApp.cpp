
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
#include "app/Fullscreen.hpp"
#include "app/Islands.hpp"
#include "app/PlayerRender.hpp"
#include "app/Wildlife.hpp"
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

int GuiApp::run(int argc, char** argv)
{
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

    const char* SYS_FONT = "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf";
    // assetBytesKept keeps the buffer alive — sf::Font reads from it after loadFromMemory
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
    // huntress was never prepared with run/turn_left/turn_right anims (neither on
    // the CDN bucket nor in the source-prep folder) — don't try to fetch them.
    for (const char* d : {"woodsprite","nomad","nyxara"})
        for (const char* f : {"idle.glb","walk.glb","run.glb","turn_left.glb","turn_right.glb","player.png"})
            gameAssets.push_back(A(std::string("characters/") + d + "/" + f));
    for (const char* f : {"idle.glb","walk.glb","player.png"})
        gameAssets.push_back(A(std::string("characters/huntress/") + f));
    for (const char* p : {"boat","cliff_rock","flower_1","flower_2","lily_pad","mapletree_1",
                          "normaltree_1","rock_1","rock_2","rock_3","tree_of_life"})
        gameAssets.push_back(A(std::string("models/") + p + ".obj"));   // audio prefetched in intro

    // loFrac/hiFrac map the wait onto a bar slice so it flows into advanceLoadingTo's range
    auto waitBar = [&](std::atomic<size_t>& done, size_t total, const char* label,
                        float loFrac = 0.f, float hiFrac = 1.f) {
        float shown = loFrac;
        while (done.load() < total) {
            float target = loFrac + (total ? (float)done.load() / total : 1.f) * (hiFrac - loFrac);
            shown += (target - shown) * 0.20f + 0.0035f;       // ease + creep
            if (shown > target + 0.08f) shown = target + 0.08f; // don't run too far ahead
            if (shown > hiFrac - 0.015f) shown = hiFrac - 0.015f;
            drawLoadingScreen(window, hudFont, shown, label);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        drawLoadingScreen(window, hudFont, hiFrac, label);     // snap to the slice's top
    };

    std::atomic<size_t> gameDone{0};
    std::thread gamePrefetch([&] { zappy::prefetch(gameAssets, &gameDone, 6); });

    {   // intro set: responsive loading bar (no freeze)
        std::atomic<size_t> d{0};
        std::thread t([&] { zappy::prefetch(introAssets, &d, 6); });
        waitBar(d, introAssets.size(), "Chargement de l'intro...");
        t.join();
    }

    MenuConfig menuCfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-p" && i + 1 < argc)      menuCfg.port = std::atoi(argv[++i]);
        else if (a == "-h" && i + 1 < argc) menuCfg.host = argv[++i];
    }
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

    GuiClient netClient;
    std::string netErr;
    bool netConnected = false;
    std::thread netThread([&] {
        netConnected = netClient.connect(menuCfg.host, menuCfg.port, netErr);
    });

    waitBar(gameDone, gameAssets.size(), "Chargement du monde...", 0.f, 0.10f);
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


    Shader skyShader ("shaders/sky.vert",         "shaders/sky.frag");
    Shader toon      ("shaders/phong.vert",        "shaders/phong.frag");
    Shader outline   ("shaders/outline.vert",      "shaders/outline.frag");
    Shader skinShader("shaders/skinned.vert",      "shaders/phong.frag");
    Shader auraShader  ("shaders/skinned_aura.vert", "shaders/aura.frag");
    Shader incantShader("shaders/incant.vert",       "shaders/incant.frag");
    Shader starShader    ("shaders/star.vert",          "shaders/star.frag");
    Shader eggShader     ("shaders/logo.vert",          "shaders/logo.frag");
    Shader waterMapShader("shaders/water_map.vert",     "shaders/water_map.frag");

    unsigned int waterMapTex = 0;
    {
        sf::Image wi;
        const std::string& wb = zappy::assetBytes("assets/screen/water.png");
        if (!wb.empty() && wi.loadFromMemory(wb.data(), wb.size())) {
            glGenTextures(1, &waterMapTex);
            glBindTexture(GL_TEXTURE_2D, waterMapTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wi.getSize().x, wi.getSize().y,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, wi.getPixelsPtr());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        } else {
            std::cerr << "Water map texture load failed - shore water falls back to flat colour\n";
        }
    }

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
    }

    // billboard quad for evolution sparkles (corners -1..1)
    unsigned int sparkVAO = 0, sparkVBO = 0;
    {
        const float quad[] = { -1.f,-1.f,  1.f,-1.f,  1.f,1.f,  -1.f,1.f };
        glGenVertexArrays(1, &sparkVAO);
        glGenBuffers(1, &sparkVBO);
        glBindVertexArray(sparkVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sparkVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    std::vector<Character> characters;
    const float CHAR_HEIGHT = 0.01f;
    std::vector<IncantationEffect> incantations;
    float incantCooldown = 25.f;  // first demo fires at 25s

    const std::string CHAR_DIR   = "assets/characters/";
    const std::string FEMALE_DIR = CHAR_DIR + "female/Meshy_AI_Huntress_of_the_Wild_biped_Animation_";
    const std::string MALE_DIR   = CHAR_DIR + "male/Meshy_AI_Ironhorn_Wanderer_biped_Animation_";

    advanceLoadingTo(window, hudFont, 0.10f, "Loading characters...");


    try {
        Character c;
        c.mesh     = SkinnedMesh::load(FEMALE_DIR + "Idle_15_withSkin.glb");
        c.mesh.loadAnimFrom(FEMALE_DIR + "Walking_withSkin.glb");           // anim index 1
        c.worldPos = {14*TILE+TILE*.5f, 0.f, 13*TILE+TILE*.5f};
        c.rotY     = 180.f;
        float meshH = c.mesh.boundsMax().y - c.mesh.boundsMin().y;
        c.scale    = (meshH > 0.001f) ? CHAR_HEIGHT / meshH : 1.f;
        c.idleAnim = 0;
        c.state    = CharState::IDLE;
        c.mesh.loadTexture(CHAR_DIR + "female/player_female.png");
        characters.push_back(std::move(c));
    } catch (std::exception& e) { std::cerr << "Female load: " << e.what() << "\n"; }
    advanceLoadingTo(window, hudFont, 0.35f, "Loaded Huntress");


    try {
        Character c;
        c.mesh     = SkinnedMesh::load(MALE_DIR + "Idle_11_withSkin.glb");  // 0 idle
        c.mesh.loadAnimFrom(MALE_DIR + "Walking_withSkin.glb");            // 1 walk
        c.mesh.loadTexture(CHAR_DIR + "male/player_male.png");
        c.worldPos = {13*TILE+TILE*.5f, 0.f, 13*TILE+TILE*.5f};
        c.rotY     = 200.f;
        float meshH = c.mesh.boundsMax().y - c.mesh.boundsMin().y;
        c.scale    = (meshH > 0.001f) ? CHAR_HEIGHT / meshH : 1.f;
        characters.push_back(std::move(c));
    } catch (std::exception& e) { std::cerr << "Male load: " << e.what() << "\n"; }
    advanceLoadingTo(window, hudFont, 0.55f, "Loaded Wanderer");

    // anim slots: 0=idle 1=walk 2=run 3=turn-left 4=turn-right (run/turn optional)
    PlayerTier tiers[4];
    const char* tierDir[4] = { "woodsprite", "nomad", "nyxara", "huntress" };
    // huntress has no run/turn_left/turn_right source files (CDN nor local prep) — skip
    bool tierHasExtraAnims[4] = { true, true, true, false };
    float globalPScale = std::getenv("ZAPPY_PSCALE")
                       ? (float)std::atof(std::getenv("ZAPPY_PSCALE")) : 0.05f / 6.f;
    for (int t = 0; t < 4; ++t) {
        std::string dir = std::string("assets/characters/") + tierDir[t] + "/";
        try {
            tiers[t].mesh = SkinnedMesh::load(dir + "idle.glb");
            tiers[t].mesh.loadAnimFrom(dir + "walk.glb");
            // optional extra anims — only attempt the fetch for tiers known to have them
            if (tierHasExtraAnims[t])
                for (const char* a : {"run.glb", "turn_left.glb", "turn_right.glb"})
                    if (!zappy::assetBytes(dir + a).empty())
                        tiers[t].mesh.loadAnimFrom(dir + a);
            tiers[t].mesh.loadTexture(dir + "player.png");
            tiers[t].scale  = globalPScale;
            tiers[t].loaded = true;
        } catch (std::exception& e) {
            std::cerr << "Tier " << tierDir[t] << " load: " << e.what() << "\n";
        }
    }
    advanceLoadingTo(window, hudFont, 0.70f, "Loaded players");

    characters.clear(); // demo chars removed — only live server players are shown


    MapData map;
    try { map = loadMap(MAP_FILE); }
    catch (std::exception& e) { std::cerr << e.what() << "\n"; return 1; }
    time_t lastMtime = fileMtime(MAP_FILE);

    // Build seamless flat ground meshes (no OBJ tile seams)
    auto landMesh  = buildGroundMesh(map.tiles, "tile",  false);
    auto cliffMesh = buildGroundMesh(map.tiles, "tile",  true);
    auto waterMesh = buildGroundMesh(map.tiles, "water", false);

    // Infinite ocean plane — large quad so no sky bleeds past the map edge
    auto oceanMesh = []() {
        const float S = 4000.f, Y = -1.82f;
        std::vector<float> v = {
            -S,Y,-S, 0,1,0,  S,Y,-S, 0,1,0,
             S,Y, S, 0,1,0, -S,Y, S, 0,1,0
        };
        std::vector<unsigned int> i = {0,1,2, 0,2,3};
        return Mesh(v, i);
    }();

    advanceLoadingTo(window, hudFont, 0.85f, "Building terrain...");

    // Pre-warm prop mesh cache
    for (auto& t : map.tiles)
        for (auto& p : t.props) getMesh(p.model);

    advanceLoadingTo(window, hudFont, 1.0f, "Ready");

    std::unordered_map<long long, std::size_t> tileIndex;
    for (std::size_t k = 0; k < map.tiles.size(); ++k)
        tileIndex[(long long)map.tiles[k].y * 100000 + map.tiles[k].x] = k;

    netThread.join();
    if (netConnected)
        std::cout << "[net] connected to " << menuCfg.host << ":"
                  << menuCfg.port << "\n";
    else
        std::cerr << "[net] connect failed: " << netErr
                  << " - running as offline viewer\n";

    // (font already loaded above for the startup menu)

    // per-player animation state (movement/turn detection from discrete updates)
    std::unordered_map<int, PlayerAnim> playerAnims;

    // evolution sparkles + tree motes (Spark defined in app/Particle.hpp)
    std::vector<Spark> sparks;
    // one independent mote pool per team island (each Tree of Life copy
    // charges/pulses on its own, same green energy, never shared/mixed)
    std::vector<std::vector<Spark>> treeMotes;
    const float EVO_DUR = 2.0f;    // length of the evolution flash (matches level-up sfx)

    bool testMode = std::getenv("ZAPPY_TESTPLAYERS") != nullptr;
    std::vector<GuiPlayer> testPlayers;
    float testTimer = 0.f;
    if (testMode) {
        int n = std::atoi(std::getenv("ZAPPY_TESTPLAYERS"));
        if (n <= 0) n = 4;
        // a horizontal row toward the front-left of the island (clear of the tree)
        int rowY = map.h / 2 + 4;
        int startX = map.w / 2 - n;
        for (int i = 0; i < n; ++i) {
            GuiPlayer p;
            p.id = 1000 + i;
            p.x = startX + i * 2;
            p.y = rowY;
            p.orientation = 3;                  // face the camera (south)
            p.level = 1;
            p.team = "demo";
            testPlayers.push_back(p);
        }
        std::cout << "[test] spawned " << n << " demo players (grow 1->8, 20s each)\n";
    }

    float cx = map.w * TILE * 0.5f;
    float cz = map.h * TILE * 0.5f;
    float camYaw   = 45.f  * 3.14159f / 180.f;
    float camPitch = 35.f  * 3.14159f / 180.f;
    int   nightOverride = 0;   // 0=auto, 1=night, 2=day
    bool  fullscreen    = false;
    float camDist  = std::max(map.w, map.h) * TILE * 1.1f; // re-framed once team count is known
    bool  camFramedForTeams = false;
    float camTargetY = 0.f;
    if (testMode && !testPlayers.empty()) {            // frame the demo row
        int n = (int)testPlayers.size();
        cx = (map.w / 2) * TILE;                       // centre of the row
        cz = (map.h / 2 + 4) * TILE;
        camTargetY = 1.6f;                             // look at body height
        camDist  = 12.f + n * 3.f;
        camPitch = 16.f * 3.14159f / 180.f;            // lower, more side-on
    }

    bool  dragging = false;
    sf::Vector2i lastMouse;

    SkinnedMesh mewMesh;
    bool  haveMew = false;
    try {
        mewMesh = SkinnedMesh::load("assets/characters/mew/scene.gltf");
        mewMesh.loadTexture("assets/characters/mew/textures/blinn1_baseColor.jpeg");
        haveMew = true;
    } catch (std::exception& e) { std::cerr << "Mew load: " << e.what() << "\n"; }

    SkinnedMesh charzMesh;
    bool  haveCharz = false;
    try {
        charzMesh = SkinnedMesh::load("assets/characters/charizard/scene.glb");
        // body_a/body_b/eyes/fire come from the GLB's embedded per-material maps
        haveCharz = true;
    } catch (std::exception& e) { std::cerr << "Charizard load: " << e.what() << "\n"; }

    SkinnedMesh moonMesh;
    bool haveMoon = false;
    try {
        moonMesh = SkinnedMesh::load("assets/models/moon/scene.gltf");
        moonMesh.loadTexture("assets/models/moon/textures/moon_baseColor.png");
        haveMoon = true;
    } catch (std::exception& e) { std::cerr << "Moon load: " << e.what() << "\n"; }

    // ── wildlife (pterodactyl + butterfly) ────────────────────────────────────
    const float TILE_F   = TILE;
    auto wfrand = [](float lo, float hi) {
        return lo + (hi - lo) * (std::rand() / (float)RAND_MAX);
    };

    // spawn helper: a point in a disk of `rad` around the island centre (cx,cz)
    auto spawnXZ = [&](float rad) {
        float a = wfrand(0.f, 6.2831853f);
        float r = rad * std::sqrt(wfrand(0.f, 1.f));
        return glm::vec2(cx + r * std::cos(a), cz + r * std::sin(a));
    };
    const float spawnR = map.w * TILE_F * 0.455f;   // ~one island radius; roaming expands later

    WildlifeGroup ptero, butterfly;
    try {
        ptero.mesh = SkinnedMesh::load("assets/wildlife/ptero/scene.gltf");
        ptero.mesh.loadTexture("assets/wildlife/ptero/textures/material0_baseColor.png");
        float ph = ptero.mesh.boundsMax().y - ptero.mesh.boundsMin().y;
        ptero.scale = (ph > 0.001f) ? (TILE_F * 0.38f) / ph : 0.01f;
        for (int i = 0; i < 6; ++i) {
            WildlifeEntity e;
            glm::vec2 p = spawnXZ(spawnR);
            e.minY   = TILE_F * 3.0f;   // soar well above the islands
            e.maxY   = TILE_F * 5.5f;
            e.pos    = { p.x, wfrand(e.minY, e.maxY), p.y };
            e.target = e.pos;
            e.speed  = wfrand(TILE_F * 0.9f, TILE_F * 1.5f);
            e.animTime = wfrand(0.f, 3.f);
            ptero.entities.push_back(e);
        }
    } catch (std::exception& e) { std::cerr << "Ptero load: " << e.what() << "\n"; }

    try {
        butterfly.mesh = SkinnedMesh::load("assets/wildlife/butterfly/scene.gltf");
        butterfly.mesh.loadTexture("assets/wildlife/butterfly/textures/butterflyMAT_baseColor.png");
        // This asset is authored flat: wide in X (wingspan), thin in Y. Normalise
        // by the wingspan so it isn't blown up, and roll it upright so the wings
        // face outward instead of lying flat to the ground.
        glm::vec3 bsz = butterfly.mesh.boundsMax() - butterfly.mesh.boundsMin();
        float span = std::max(bsz.x, bsz.z);
        butterfly.scale     = (span > 0.001f) ? (TILE_F * 0.28f) / span : 0.005f;
        butterfly.baseRoll  = 90.f;
        butterfly.alphaCutout = true;   // wing silhouette is in the texture alpha
        for (int i = 0; i < 25; ++i) {
            WildlifeEntity e;
            glm::vec2 p = spawnXZ(spawnR);
            e.minY   = TILE_F * 0.10f;  // flit just above ground level
            e.maxY   = TILE_F * 0.55f;
            e.pos    = { p.x, wfrand(e.minY, e.maxY), p.y };
            e.target = e.pos;
            e.speed  = wfrand(TILE_F * 0.5f, TILE_F * 1.1f);
            e.animTime = wfrand(0.f, 2.f);
            butterfly.entities.push_back(e);
        }
    } catch (std::exception& e) { std::cerr << "Butterfly load: " << e.what() << "\n"; }

    // lowest Mew may descend to — keeps his whole body above the map surface
    const float MEW_FLOOR = std::getenv("ZAPPY_MEW_FLOOR")
                          ? (float)std::atof(std::getenv("ZAPPY_MEW_FLOOR")) : 1.0f;
    const float MEW_SCALE = std::getenv("ZAPPY_MEW_SCALE")
                          ? (float)std::atof(std::getenv("ZAPPY_MEW_SCALE")) : 0.27f;
    const glm::vec3 TREE_POS = {22*TILE, 0.f, 22*TILE};

    // pose figée à l'arrêt : une frame de vol (pas la frame 0 qui est verticale)
    float       mewIdlePose = std::getenv("ZAPPY_MEW_POSE")
                          ? (float)std::atof(std::getenv("ZAPPY_MEW_POSE")) : 2.0f;
    const float mewAnimDur = (haveMew && mewMesh.animCount() > 0)
                          ? mewMesh.animDuration(0) : 11.f;
    bool        adminPrompt = false;
    bool        adminEatNext = false;
    bool        adminMode   = false;
    bool        charzMode   = false;   // player has control of Charizard
    std::string adminInput;
    glm::vec3   mewPos(cx, 6.f, cz);
    float       mewYaw = 0.f, mewAnimTime = mewIdlePose;
    bool        mewMoving = false;
    float       mewFlyTimer = 0.f;
    float       mewMeowTimer = 10.f;

    // ── Charizard admin mount ──────────────────────────────────────────────────
    // Behaves exactly like Mew (password -> control it), the difference being it
    // can land and walk/run on the ground instead of only flying.
    const float CHARZ_SCALE    = std::getenv("ZAPPY_CHARZ_SCALE")
                               ? (float)std::atof(std::getenv("ZAPPY_CHARZ_SCALE")) : 0.9f;
    const float CHARZ_GROUND_Y = std::getenv("ZAPPY_CHARZ_GROUND_Y")
                              ? (float)std::atof(std::getenv("ZAPPY_CHARZ_GROUND_Y")) : 0.0f;
    // anim indices (verified against scene.glb's 83 clips)
    // NB: anim 0 (defaultwait) is a *hovering* pose for Charizard, so the
    // grounded idle defaults to battlewait01_loop (1, upright stance). Override
    const int CA_IDLE     = std::getenv("ZAPPY_CHARZ_IDLE")
                         ? std::atoi(std::getenv("ZAPPY_CHARZ_IDLE")) : 1;  // ground idle: battlewait01_loop
    const int CA_FLY_IDLE = std::getenv("ZAPPY_CHARZ_FLY_IDLE")
                         ? std::atoi(std::getenv("ZAPPY_CHARZ_FLY_IDLE")) : 49; // flying idle: battlewait01_loop (hover)
    const float CHARZ_ANIM_SPEED = std::getenv("ZAPPY_CHARZ_ANIM_SPEED")
                         ? (float)std::atof(std::getenv("ZAPPY_CHARZ_ANIM_SPEED")) : 1.5f;
    const int CA_WALK       = 7;   // ground walk01_loop
    const int CA_RUN        = 8;   // ground run01_loop
    const int CA_TURN_L     = 4;   // ground turn left
    const int CA_TURN_R     = 5;   // ground turn right
    const int CA_AIR_WALK   = 54;  // air walk01_loop
    const int CA_AIR_RUN    = 55;  // air run01_loop
    const int CA_AIR_TURN_L = 53;  // air turn left
    const int CA_AIR_TURN_R = 52;  // air turn right
    const int CA_TAKEOFF    = 12;  // jumpup01_start        (one-shot)
    const int CA_FLY        = 13;  // jumpup01_loop         (airborne)
    const int CA_FALL       = 62;  // 20153_jumpdown01_loop (aerial descent)
    const int CA_LAND_A     = 63;  // 20155_land02          (aerial phase, plays first)
    const int CA_LAND_B     = 16;  // 00155_land02          (ground phase, plays second)
    const float CHARZ_FLY_Y = CHARZ_GROUND_Y + 6.f;
    const float caTakeoffDur = (haveCharz && charzMesh.animCount() > CA_TAKEOFF)
                             ? charzMesh.animDuration(CA_TAKEOFF) : 0.8f;
    // jumpup01_start is only ~0.12s (a crouch-prep pose); the actual ascent lives
    // in jumpup01_loop. Spread the 6-unit climb over a longer window so it reads
    // as a launch instead of teleporting straight up.
    const float caClimbDur = 0.9f;
    const float caLandADur   = (haveCharz && charzMesh.animCount() > CA_LAND_A)
                             ? charzMesh.animDuration(CA_LAND_A) : 0.5f;
    const float caLandBDur   = (haveCharz && charzMesh.animCount() > CA_LAND_B)
                             ? charzMesh.animDuration(CA_LAND_B) : 0.7f;
    enum class CharzMove { GROUNDED, TAKEOFF, FLYING, LANDING_A, LANDING_B };
    CharzMove   charzMove     = CharzMove::GROUNDED;
    float       charzMoveT    = 0.f;   // time inside a one-shot transition
    glm::vec3   charzPos      (cx, CHARZ_GROUND_Y, cz);
    float       charzYaw      = 0.f;
    float       charzAnimTime = 0.f;
    int         charzAnimIdx  = CA_IDLE;
    int         charzPrevAnim = -1;
    float       charzBobAmp   = 0.f;

    Constellation constellation;
    float admCamYaw = 0.f, admCamPitch = 0.32f; // 3rd-person cam offset around Mew


    AudioManager gameAudio;
    gameAudio.init("assets/audio");
    if (netConnected) gameAudio.playMusic("game_music", true, 35.f);
    unsigned long lastLogTotal = 0;   // for draining new action events into SFX
    float pickupCooldown = 0.f;       // throttles the (frequent) grab blip

    sf::Clock clock;
    float elapsed  = 0.f;
    const float CHARGE_DURATION  =  8.f;   // seconds to charge up
    const float IDLE_DURATION    = 52.f;   // seconds idle before next charge
    const float WAVE_SPEED       = 14.f;   // world units / second
    const float WAVE_MAXR        = map.w * TILE * 1.1f;

    enum class PulseState { IDLE, CHARGING, RELEASING };
    PulseState pulseState  = PulseState::IDLE;
    PulseState prevPulse   = PulseState::IDLE;   // edge-detect charge→release
    float      pulseTimer  = 0.f;   // time spent in current state
    float      chargeT     = 0.f;   // 0→1 charge progress
    float      treeGlow    = 0.f;   // foliage emissive: tracks charge, fades out after release
    float      waveRadius  = 0.f;

    HudOverlay hud;
    hud.init(hudFont);

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        elapsed  += dt;

        hud.update(dt);                         // smooth sidebar slide

        // tick down the resource-reset countdown (server resyncs it via "sst")
        if (netConnected && netClient.state().secondsToReset > 0.f)
            netClient.state().secondsToReset =
                std::max(0.f, netClient.state().secondsToReset - dt);

        // while a player popup is open, poll its inventory (~3×/s) so counts stay live
        int pollId = hud.inventoryPollTarget(dt);
        if (pollId >= 0 && netConnected)
            netClient.send("pin #" + std::to_string(pollId));

        if (netConnected) {
            const GuiState& gs = netClient.state();
            pickupCooldown = std::max(0.f, pickupCooldown - dt);
            unsigned long total = gs.logTotal;
            unsigned long newCount = total - lastLogTotal;
            if (newCount > gs.log.size()) newCount = (unsigned long)gs.log.size();
            for (std::size_t i = gs.log.size() - newCount; i < gs.log.size(); ++i) {
                int k = gs.log[i].kind;
                if (k == 2)      gameAudio.play("death",  85.f);   // pdi
                else if (k == 3) gameAudio.play("incant", 80.f);   // pic
                else if (k == 0 && pickupCooldown <= 0.f) {        // pgt — subtle, throttled
                    gameAudio.play("pickup", 32.f);
                    pickupCooldown = 0.18f;
                }
            }
            lastLogTotal = total;
        }

        if (netConnected) {
            netClient.poll();
            netConnected = netClient.connected();
            const GuiState& gs = netClient.state();
            if (!camFramedForTeams && !gs.teams.empty()) { // re-frame once team count is known
                float ext = worldExtentRadius(1, map.w * TILE * 0.455f);
                camDist = ext * 2.42f;
                camFramedForTeams = true;
            }
            if (gs.sizeKnown) {
                for (auto& t : map.tiles)
                    for (int i = 0; i < 7; ++i) t.res[i] = 0;
                const int insetT = 7;
                int landX = std::max(1, map.w - 2 * insetT);
                int landY = std::max(1, map.h - 2 * insetT);
                for (int y = 0; y < gs.height; ++y)
                    for (int x = 0; x < gs.width; ++x) {
                        int itx = insetT + x * landX / std::max(1, gs.width);
                        int ity = insetT + y * landY / std::max(1, gs.height);
                        auto it = tileIndex.find((long long)ity * 100000 + itx);
                        if (it == tileIndex.end()) continue;
                        const GuiTile& st = gs.tileAt(x, y);
                        for (int i = 0; i < 7; ++i)
                            map.tiles[it->second].res[i] += st.res[i];
                    }
            }
            static bool dbg = (std::getenv("ZAPPY_DEBUG") != nullptr);
            static int dbgFrame = 0;
            if (dbg && (++dbgFrame % 120 == 0)) {
                long totalRes = 0;
                for (auto& t : map.tiles) for (int i = 0; i < 7; ++i) totalRes += t.res[i];
                std::cerr << "[dbg] grid " << gs.width << "x" << gs.height
                          << "  teams " << gs.teams.size()
                          << "  players " << gs.players.size()
                          << "  eggs " << gs.eggs.size()
                          << "  mapRes " << totalRes << "\n";
            }
        }

        if (testMode) { // ZAPPY_TESTSEC controls seconds per level phase
            static float phaseSec = std::getenv("ZAPPY_TESTSEC")
                                  ? (float)std::atof(std::getenv("ZAPPY_TESTSEC")) : 1.f;
            testTimer += dt;
            int lvl = ((int)(testTimer / phaseSec)) % 9;   // 0(egg),1,...,8, loop
            for (auto& p : testPlayers) p.level = lvl;
        }

        pulseTimer += dt;
        switch (pulseState) {
        case PulseState::IDLE: {
            chargeT = 0.f;
            // online: charge on last CHARGE_DURATION seconds before resource reset; offline: self-timed
            float sr = netConnected ? netClient.state().secondsToReset : -1.f;
            bool trigger = (sr >= 0.f) ? (sr <= CHARGE_DURATION)
                                       : (pulseTimer >= IDLE_DURATION);
            if (trigger) {
                pulseState = PulseState::CHARGING;
                pulseTimer = 0.f;
            }
            break;
        }
        case PulseState::CHARGING:
            chargeT = glm::clamp(pulseTimer / CHARGE_DURATION, 0.f, 1.f);
            if (pulseTimer >= CHARGE_DURATION) {
                pulseState = PulseState::RELEASING;
                pulseTimer = 0.f;
                waveRadius = 0.f;
                for (auto& ch : characters) ch.auraTriggered = false;
            }
            break;
        case PulseState::RELEASING:
            chargeT    = 0.f;
            waveRadius += WAVE_SPEED * dt;
            for (auto& ch : characters) {
                if (!ch.auraTriggered) {
                    float dist = glm::length(glm::vec2(ch.worldPos.x - TREE_POS.x,
                                                       ch.worldPos.z - TREE_POS.z));
                    if (waveRadius >= dist) {
                        ch.auraTimer     = 30.f;
                        ch.auraTriggered = true;
                    }
                }
            }
            if (waveRadius >= WAVE_MAXR) {
                pulseState = PulseState::IDLE;
                pulseTimer = 0.f;
            }
            break;
        }
        if (pulseState == PulseState::CHARGING)
            treeGlow = chargeT;
        else
            treeGlow = std::max(0.f, treeGlow - dt * 0.5f);   // ~2s fade-out

        for (auto& ch : characters)
            if (ch.auraTimer > 0.f) ch.auraTimer -= dt;

        incantCooldown -= dt;
        if (incantCooldown <= 0.f && !characters.empty() &&
            pulseState == PulseState::IDLE) {
            IncantationEffect fx;
            fx.pos = characters[0].worldPos; fx.pos.y = 0.f;
            incantations.push_back(fx);
            incantCooldown = 55.f;
        }
        bool anyIncant = false;
        for (auto& ix : incantations) {
            ix.timer += dt;
            if (ix.timer >= INCANT_DUR - 0.5f) ix.success = true;
            anyIncant = true;
        }
        incantations.erase(
            std::remove_if(incantations.begin(), incantations.end(),
                [](const IncantationEffect& ix){ return ix.timer >= INCANT_DUR; }),
            incantations.end());
        for (auto& ch : characters) ch.incanting = anyIncant;

        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed)
                window.close();
            // The window manager can resize the window out-of-band (maximize
            // toggle, dragged edge): SFML's 2D view does NOT auto-follow, so
            // without this the HUD stays mapped to the old size and drifts
            // off-screen. The 3D pass already re-reads window.getSize() each
            // frame for glViewport, so only the 2D view needs resetting here.
            if (ev.type == sf::Event::Resized)
                window.setView(sf::View(sf::FloatRect(0.f, 0.f,
                    (float)ev.size.width, (float)ev.size.height)));

            if (adminPrompt) {
                if (ev.type == sf::Event::TextEntered) {
                    if (adminEatNext) {                                  // le 'a' d'ouverture
                        adminEatNext = false;
                    } else if (ev.text.unicode == 8) {                   // backspace
                        if (!adminInput.empty()) adminInput.pop_back();
                    } else if (ev.text.unicode >= 32 && ev.text.unicode < 127 &&
                               adminInput.size() < 32) {
                        adminInput += (char)ev.text.unicode;
                    }
                }
                if (ev.type == sf::Event::KeyPressed) {
                    if (ev.key.code == sf::Keyboard::Enter) {
                        if (adminInput == "char6006") {                  // déverrouillage Charizard
                            charzMode = true;
                            charzMove = CharzMove::FLYING; charzMoveT = 0.f;
                            charzPos  = glm::vec3(
                                cx + std::sin(camYaw) * 12.f,
                                12.f,
                                cz + std::cos(camYaw) * 12.f);
                            charzYaw    = camYaw + glm::radians(180.f);
                            charzAnimTime = 0.f; charzAnimIdx = CA_IDLE;
                            charzBobAmp   = 0.08f;
                            admCamYaw   = 0.f;
                            admCamPitch = 0.55f;  // look slightly down so world is visible
                            gameAudio.play("reveal", 90.f);
                        } else if (adminInput == "admin1234") {          // déverrouillage Mew
                            adminMode = true;
                            mewPos = glm::vec3(cx, camTargetY + 4.f, cz);
                            mewYaw = camYaw;
                            gameAudio.play("reveal", 90.f);
                            gameAudio.play("mew_cry", 80.f);               // little chirp on reveal
                            gameAudio.playLoop("mew_float", 80.f);         // ethereal hover hum
                        }
                        adminPrompt = false; adminInput.clear();
                    } else if (ev.key.code == sf::Keyboard::Escape) {
                        adminPrompt = false; adminInput.clear();
                    }
                }
                continue;                                                // on avale la saisie
            }

            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Escape)
                window.close();
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::A && !adminMode && !charzMode) {
                adminPrompt = true; adminInput.clear(); adminEatNext = true;
                continue;
            }
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Q && (adminMode || charzMode)) {
                adminMode = false;
                charzMode = false;
                gameAudio.stopLoop();
            }
            // scrub de la pose idle (O / P) pour choisir la frame de repos
            if (adminMode && ev.type == sf::Event::KeyPressed &&
                (ev.key.code == sf::Keyboard::O || ev.key.code == sf::Keyboard::P)) {
                mewIdlePose += (ev.key.code == sf::Keyboard::P ? 0.5f : -0.5f);
                if (mewIdlePose < 0.f)         mewIdlePose = 0.f;
                if (mewIdlePose > mewAnimDur)  mewIdlePose = mewAnimDur;
                mewAnimTime = mewIdlePose;                               // applique tout de suite
                std::cerr << "[mew] pose=" << mewIdlePose << "\n";
            }

            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::R) {
                if (adminMode || charzMode) {          // reset the 3rd-person view
                    admCamYaw = 0.f; admCamPitch = 0.32f;
                } else {
                    camYaw = 45.f * 3.14159f / 180.f;
                    camPitch = 35.f * 3.14159f / 180.f;
                    float ext = worldExtentRadius(1, map.w * TILE * 0.455f);
                    camDist = ext * 2.42f;
                }
            }
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Tab)
                hud.toggle();
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::N)
                nightOverride = (nightOverride + 1) % 3;
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::F11) {
                fullscreen = !fullscreen;
                setWindowFullscreen(window.getSystemHandle(), fullscreen);
            }

            if (ev.type == sf::Event::MouseButtonPressed)
                gameAudio.play("click", 65.f);                       // son à chaque clic

            if (ev.type == sf::Event::MouseButtonPressed &&
                ev.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2i mp = sf::Mouse::getPosition(window);
                if (!hud.handleClick(mp)) {                          // empty space → orbit
                    dragging = true;
                    lastMouse = mp;
                }
                if (hud.consumeFullscreenClick()) {                  // top-left square icon
                    fullscreen = !fullscreen;
                    setWindowFullscreen(window.getSystemHandle(), fullscreen);
                }
            }
            if (ev.type == sf::Event::MouseButtonReleased)
                dragging = false;

            if (ev.type == sf::Event::MouseMoved && dragging) {
                sf::Vector2i cur = sf::Mouse::getPosition(window);
                if (adminMode || charzMode) {                 // orbit around Mew / Charizard
                    admCamYaw   -= (cur.x - lastMouse.x) * 0.008f;
                    admCamPitch += (cur.y - lastMouse.y) * 0.006f;
                    admCamPitch  = glm::clamp(admCamPitch, -1.20f, 1.40f);
                } else {
                    camYaw   += (cur.x - lastMouse.x) * 0.008f;
                    camPitch += (cur.y - lastMouse.y) * 0.006f;
                    camPitch  = glm::clamp(camPitch, 0.15f, 1.48f);
                }
                lastMouse = cur;
            }

            if (ev.type == sf::Event::MouseWheelScrolled) {
                sf::Vector2i mp = sf::Mouse::getPosition(window);
                if (!hud.handleScroll(mp, ev.mouseWheelScroll.delta)) {
                    float ext = worldExtentRadius(1, map.w * TILE * 0.455f);
                    float maxDist = std::max(120.f, ext * 3.0f);
                    camDist = glm::clamp(camDist - ev.mouseWheelScroll.delta * 1.5f,
                                         4.f, maxDist);
                }
            }
        }

        time_t mt = fileMtime(MAP_FILE);
        if (mt != lastMtime) {
            try {
                map       = loadMap(MAP_FILE);
                landMesh  = buildGroundMesh(map.tiles, "tile",  false);
                cliffMesh = buildGroundMesh(map.tiles, "tile",  true);
                waterMesh = buildGroundMesh(map.tiles, "water", false);
                for (auto& t : map.tiles)
                    for (auto& p : t.props) getMesh(p.model);
                cx = map.w * TILE * 0.5f;
                cz = map.h * TILE * 0.5f;
                lastMtime = mt;
                std::cout << "Map reloaded\n";
            } catch (std::exception& e) {
                std::cerr << "Reload failed: " << e.what() << "\n";
            }
        }

        if (adminMode) {
            const float spd = 16.f, turn = 1.9f, vspd = 12.f;
            glm::vec3 fwd(std::sin(mewYaw), 0.f, std::cos(mewYaw));
            mewMoving = false;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::W)) { mewPos += fwd * spd * dt; mewMoving = true; }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) { mewPos -= fwd * spd * dt; mewMoving = true; }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  mewYaw += turn * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) mewYaw -= turn * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Z)) { mewPos.y += vspd * dt; mewMoving = true; }  // up
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) { mewPos.y -= vspd * dt; mewMoving = true; }  // down
            if (mewPos.y < MEW_FLOOR) mewPos.y = MEW_FLOOR;   // can't go under the map
            if (mewMoving) {
                mewAnimTime += dt;
            } else {
                float dur   = mewAnimDur > 0.01f ? mewAnimDur : 11.f;
                float phase = std::fmod(mewAnimTime, dur); if (phase < 0) phase += dur;
                float toTarget = std::fmod(mewIdlePose - phase + dur, dur);
                float step = dt * 3.f;                            // retour ~3x plus rapide
                if (toTarget <= step) mewAnimTime = mewIdlePose;  // arrivé : on fige
                else                  mewAnimTime += step;        // continue l'anim, vite
            }

            if (mewMoving) {
                mewFlyTimer -= dt;
                if (mewFlyTimer <= 0.f) { gameAudio.play("mew_fly", 80.f); mewFlyTimer = 0.7f; }
                gameAudio.setLoopVolume(80.f);
            } else {
                mewFlyTimer = 0.f;
                gameAudio.setLoopVolume(80.f);
            }

            mewMeowTimer -= dt;
            if (mewMeowTimer <= 0.f) {
                const char* meows[3] = { "mew_meow_a", "mew_meow_b", "mew_meow_c" };
                gameAudio.play(meows[std::rand() % 3], 80.f);
                mewMeowTimer = 9.f + (std::rand() % 40) / 10.f;   // ~9-13 s
            }

        }
        // ── Charizard control: realistic takeoff / flight / landing ────────
        if (charzMode) {
            const float turn = 2.0f;
            glm::vec3 fwd(std::sin(charzYaw), 0.f, std::cos(charzYaw));
            bool fwd_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Up)   || sf::Keyboard::isKeyPressed(sf::Keyboard::W);
            bool bck_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Down);
            bool run_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
            bool up_k  = sf::Keyboard::isKeyPressed(sf::Keyboard::Z);
            bool dn_k  = sf::Keyboard::isKeyPressed(sf::Keyboard::S);
            bool moving = fwd_k || bck_k;

            // turning is always allowed
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  charzYaw += turn * dt;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) charzYaw -= turn * dt;

            bool lft_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Left);
            bool rgt_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Right);

            switch (charzMove) {
            case CharzMove::GROUNDED: {
                float spd = run_k ? 18.f : 6.f;
                if (fwd_k) charzPos += fwd * spd * dt;
                if (bck_k) charzPos -= fwd * spd * dt;
                charzPos.y = CHARZ_GROUND_Y;
                if      (moving && run_k) { charzAnimIdx = CA_RUN;    charzBobAmp = 0.02f; }
                else if (moving)          { charzAnimIdx = CA_WALK;   charzBobAmp = 0.03f; }
                else if (lft_k)           { charzAnimIdx = CA_TURN_L; charzBobAmp = 0.01f; }
                else if (rgt_k)           { charzAnimIdx = CA_TURN_R; charzBobAmp = 0.01f; }
                else                      { charzAnimIdx = CA_IDLE;   charzBobAmp = 0.04f; }
                if (up_k) {
                    charzMove = CharzMove::TAKEOFF; charzMoveT = 0.f;
                    charzAnimIdx = CA_TAKEOFF; charzAnimTime = 0.f; charzBobAmp = 0.f;
                }
                break;
            }
            case CharzMove::TAKEOFF: {
                charzMoveT += dt;
                charzBobAmp = 0.f;
                if (charzMoveT < caTakeoffDur) {
                    // crouch-prep pose, still planted on the ground
                    charzPos.y    = CHARZ_GROUND_Y;
                    charzAnimIdx  = CA_TAKEOFF;
                    charzAnimTime = std::min(charzMoveT, caTakeoffDur - 0.01f);
                } else {
                    // ascent: jumpup loop while easing up to flight altitude
                    float k = glm::clamp((charzMoveT - caTakeoffDur) / caClimbDur,
                                         0.f, 1.f);
                    k = k * k * (3.f - 2.f * k);           // smoothstep launch
                    charzPos.y    = CHARZ_GROUND_Y + (CHARZ_FLY_Y - CHARZ_GROUND_Y) * k;
                    charzAnimIdx  = CA_FLY;                 // jumpup01_loop
                    charzAnimTime = charzMoveT - caTakeoffDur;
                }
                if (charzMoveT >= caTakeoffDur + caClimbDur) {
                    charzMove = CharzMove::FLYING; charzAnimTime = 0.f;
                }
                break;
            }
            case CharzMove::FLYING: {
                float spd = run_k ? 30.f : 18.f, vspd = 14.f;
                if (fwd_k) charzPos += fwd * spd * dt;
                if (bck_k) charzPos -= fwd * spd * dt;
                if (up_k)  charzPos.y += vspd * dt;
                if (dn_k)  charzPos.y -= vspd * dt;
                if      (moving && run_k) { charzAnimIdx = CA_AIR_RUN;   charzBobAmp = 0.04f; }
                else if (moving)          { charzAnimIdx = CA_AIR_WALK;  charzBobAmp = 0.06f; }
                else if (dn_k)            { charzAnimIdx = CA_FALL;      charzBobAmp = 0.04f; }
                else if (up_k)            { charzAnimIdx = CA_FLY;       charzBobAmp = 0.04f; }
                else if (lft_k)           { charzAnimIdx = CA_AIR_TURN_L;charzBobAmp = 0.04f; }
                else if (rgt_k)           { charzAnimIdx = CA_AIR_TURN_R;charzBobAmp = 0.04f; }
                else                      { charzAnimIdx = CA_FLY_IDLE;  charzBobAmp = 0.08f; }
                if (charzPos.y <= CHARZ_GROUND_Y) {
                    charzPos.y = CHARZ_GROUND_Y;
                    charzMove = CharzMove::LANDING_A; charzMoveT = 0.f;
                    charzAnimIdx = CA_LAND_A; charzAnimTime = 0.f; charzBobAmp = 0.f;
                }
                break;
            }
            case CharzMove::LANDING_A: {  // aerial land phase (plays first)
                charzMoveT += dt;
                charzPos.y = CHARZ_GROUND_Y;
                charzAnimIdx = CA_LAND_A;
                charzAnimTime = std::min(charzMoveT, caLandADur - 0.01f);
                charzBobAmp = 0.f;
                if (charzMoveT >= caLandADur) {
                    charzMove = CharzMove::LANDING_B; charzMoveT = 0.f;
                    charzAnimIdx = CA_LAND_B; charzAnimTime = 0.f;
                }
                break;
            }
            case CharzMove::LANDING_B: {  // ground land phase (plays second)
                charzMoveT += dt;
                charzPos.y = CHARZ_GROUND_Y;
                charzAnimIdx = CA_LAND_B;
                charzAnimTime = std::min(charzMoveT, caLandBDur - 0.01f);
                charzBobAmp = 0.f;
                if (charzMoveT >= caLandBDur) {
                    charzMove = CharzMove::GROUNDED; charzAnimTime = 0.f;
                }
                break;
            }
            }

            if (charzMove == CharzMove::GROUNDED || charzMove == CharzMove::FLYING) {
                // reset clock on animation change so every clip starts from frame 0
                if (charzAnimIdx != charzPrevAnim) {
                    charzAnimTime = 0.f;
                    charzPrevAnim = charzAnimIdx;
                }
                charzAnimTime += dt * CHARZ_ANIM_SPEED;
                // wrap to keep time in [0, duration) for a clean loop
                if (haveCharz && charzMesh.animCount() > charzAnimIdx) {
                    float dur = charzMesh.animDuration(charzAnimIdx);
                    if (dur > 0.01f && charzAnimTime >= dur)
                        charzAnimTime = std::fmod(charzAnimTime, dur);
                }
            }
        }

        constellation.tick(sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && !adminPrompt,
                            adminMode, dt);

        glm::vec3 target(cx, camTargetY, cz);
        glm::vec3 camPos = target + glm::vec3(
            camDist * std::cos(camPitch) * std::sin(camYaw),
            camDist * std::sin(camPitch),
            camDist * std::cos(camPitch) * std::cos(camYaw));

        sf::Vector2u ws = window.getSize();
        glViewport(0, 0, ws.x, ws.y);   // suit la taille (fenêtré <-> plein écran)
        float aspect = (float)ws.x / ws.y;
        glm::mat4 proj = glm::perspective(glm::radians(38.f), aspect, 0.1f, 8000.f);

        glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0,1,0));

        if (adminMode) {
            float by = mewPos.y;
            target = glm::vec3(mewPos.x, by + 1.0f, mewPos.z);
            float ay = mewYaw + admCamYaw;
            float ap = glm::clamp(admCamPitch, -1.20f, 1.40f);
            float dist = 9.f, horiz = dist * std::cos(ap);
            camPos = glm::vec3(mewPos.x - std::sin(ay) * horiz,
                               by + 1.0f + dist * std::sin(ap),
                               mewPos.z - std::cos(ay) * horiz);
            view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));
        }
        if (charzMode) {   // camera follows the dragon
            bool onGround = charzPos.y <= CHARZ_GROUND_Y + 0.05f;
            float by = charzPos.y;
            float headH = onGround ? 1.8f : 2.5f;
            target = glm::vec3(charzPos.x, by + headH, charzPos.z);
            float ay = charzYaw + admCamYaw;
            float ap = glm::clamp(admCamPitch, -1.20f, 1.40f);
            float camD = 16.f;
            float horiz = camD * std::cos(ap);
            camPos = glm::vec3(charzPos.x - std::sin(ay) * horiz,
                               by + headH + camD * std::sin(ap),
                               charzPos.z - std::cos(ay) * horiz);
            view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));
        }

        DayNight dn = computeDayNight(elapsed, nightOverride);
        float     nightF   = dn.nightF,   ambScale = dn.ambScale;
        glm::vec3 skySunDir = dn.skySunDir, moonDir = dn.moonDir, sunDir = dn.sunDir;
        glm::vec3 lightCol  = dn.lightCol,  skyTop  = dn.skyTop,  skyHor = dn.skyHor;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glm::mat4 invVP  = glm::inverse(proj * view);
        skyShader.use();
        skyShader.setVec3("uSkyTop",      skyTop.x, skyTop.y, skyTop.z);
        skyShader.setVec3("uSkyHorizon",  skyHor.x, skyHor.y, skyHor.z);
        skyShader.setMat4("uInvViewProj", glm::value_ptr(invVP));
        skyShader.setVec3("uCamPos",      camPos.x, camPos.y, camPos.z);
        skyShader.setVec3("uSunDir",      skySunDir.x, skySunDir.y, skySunDir.z);
        skyShader.setVec3("uMoonDir",     moonDir.x, moonDir.y, moonDir.z);
        skyShader.setFloat("uSunStrength",  1.f - nightF);
        skyShader.setFloat("uMoonStrength", nightF);
        glBindVertexArray(skyVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        toon.use();
        toon.setMat4("uView",              glm::value_ptr(view));
        toon.setMat4("uProj",              glm::value_ptr(proj));
        toon.setVec3("uCamPos",            camPos.x, camPos.y, camPos.z);
        toon.setVec3("uDirLightDir",       sunDir.x, sunDir.y, sunDir.z);
        toon.setVec3("uDirLightColor",     lightCol.x, lightCol.y, lightCol.z);
        toon.setVec3("uPointLight0Pos",    0.f, 10.f, 0.f);
        toon.setVec3("uPointLight0Color",  0.f, 0.f, 0.f);
        toon.setVec3("uPointLight1Pos",    0.f, 10.f, 0.f);
        toon.setVec3("uPointLight1Color",  0.f, 0.f, 0.f);
        toon.setFloat("uShininess",        32.f);
        toon.setFloat("uAmbientStrength",  0.10f * ambScale);
        toon.setFloat("uAlpha",       1.0f);
        toon.setFloat("uTime",        elapsed);
        toon.setBool ("uWave",           false);
        toon.setBool ("uSway",           false);
        toon.setBool ("uUseVertexColor", false);
        toon.setBool ("uUseTexture",    false);
        toon.setBool ("uMultiColor",    false);
        toon.setBool ("uCrystal",     false);
        toon.setBool ("uMultiColor",  false);
        toon.setBool ("uCircleClip",  false);
        toon.setBool ("uShoreBlend",  false);
        toon.setFloat("uShoreInnerR", 0.f);
        toon.setFloat("uShoreWidth",  1.f);

        glm::vec2 sunXZ = glm::normalize(glm::vec2(sunDir.x, sunDir.z) + 1e-6f);
        drawGround(toon, waterMapShader, waterMapTex, view, proj, elapsed, sunXZ,
                   landMesh, oceanMesh, waterMesh, map, cx, cz, 1);

        drawProps(toon, outline, map, view, proj, elapsed, ambScale, treeGlow,
                 cx, cz, 1);

        drawResources(eggShader, gemTex, map, sunDir, lightCol, ambScale, view, proj, elapsed,
                      cx, cz, 1);

        drawFood(eggShader, foodMesh, map, sunDir, lightCol, ambScale, view, proj, elapsed,
                cx, cz, 1);

        {
            PlayerRenderCtx prCtx{
                tiers, playerAnims, sparks,
                testMode, netConnected, testPlayers, netClient, map, EVO_DUR, gameAudio,
                adminMode, haveMew, mewMesh, mewAnimTime, mewMoving, mewPos, mewYaw, MEW_SCALE,
                (haveCharz && charzMode), charzMesh, charzAnimIdx, charzAnimTime,
                (charzMove == CharzMove::GROUNDED || charzMove == CharzMove::FLYING),
                charzBobAmp, charzPos, charzYaw, CHARZ_SCALE,
                haveMoon, moonMesh, moonDir,
                eggMeshTex.get(),
                hud, constellation
            };
            updateAndDrawPlayers(prCtx, skinShader, auraShader, toon, starShader, eggShader,
                                 sparkVAO, view, proj, camPos, sunDir, lightCol, ambScale,
                                 nightF, elapsed, dt);
        }

        drawCharacters(characters, skinShader, auraShader, view, proj,
                       camPos, sunDir, lightCol, ambScale, elapsed, dt);

        {
            bool charging     = (pulseState == PulseState::CHARGING);
            bool justReleased = (pulseState == PulseState::RELEASING &&
                                 prevPulse  != PulseState::RELEASING);
            prevPulse = pulseState;

            if (treeMotes.empty()) treeMotes.resize(1);
            drawTreeMotes(treeMotes[0], charging, chargeT, justReleased, TREE_POS,
                          starShader, sparkVAO, view, proj, elapsed, dt);
        }

        constellation.updateMewEnergy(adminMode, dt, mewPos, elapsed);
        constellation.renderMewEnergy(starShader, sparkVAO, view, proj, elapsed);
        drawIncantations(incantations, incantShader, view, proj, camPos, elapsed);
        drawClouds(toon, map, elapsed);

        // keep the wildlife roaming over the island cluster, not the open ocean
        float wfRadius = worldExtentRadius(1, map.w * TILE * 0.455f);
        updateWildlife(ptero,     dt, cx, cz, wfRadius);
        updateWildlife(butterfly, dt, cx, cz, wfRadius);
        drawWildlife(ptero,     skinShader, view, proj, camPos, skySunDir, lightCol, ambScale, elapsed);
        drawWildlife(butterfly, skinShader, view, proj, camPos, skySunDir, lightCol, ambScale, elapsed);

        window.resetGLStates();
        hud.draw(window, netConnected ? &netClient.state() : nullptr,
                 menuCfg.host, menuCfg.port, map.w, map.h);

        if (adminPrompt) {
            sf::RectangleShape dim(sf::Vector2f((float)ws.x, (float)ws.y));
            dim.setFillColor(sf::Color(0, 0, 0, 130)); window.draw(dim);
            float bw = 380.f, bh = 124.f;
            float bx = ws.x / 2.f - bw / 2.f, byp = ws.y / 2.f - bh / 2.f;
            sf::RectangleShape box(sf::Vector2f(bw, bh));
            box.setPosition(bx, byp);
            box.setFillColor(sf::Color(16, 20, 30, 245));
            box.setOutlineColor(sf::Color(120, 170, 255, 220)); box.setOutlineThickness(2.f);
            window.draw(box);
            sf::Text title; title.setFont(hudFont); title.setString("MODE ADMIN");
            title.setCharacterSize(20); title.setFillColor(sf::Color(150, 200, 255));
            title.setPosition(bx + 18.f, byp + 14.f); window.draw(title);
            sf::RectangleShape field(sf::Vector2f(bw - 36.f, 34.f));
            field.setPosition(bx + 18.f, byp + 48.f);
            field.setFillColor(sf::Color(8, 10, 16, 255));
            field.setOutlineColor(sf::Color(90, 120, 170, 200)); field.setOutlineThickness(1.f);
            window.draw(field);
            std::string mask(adminInput.size(), '*');
            bool blink = ((int)(elapsed * 2.f)) % 2 == 0;
            sf::Text inp; inp.setFont(hudFont); inp.setString(mask + (blink ? "_" : ""));
            inp.setCharacterSize(20); inp.setFillColor(sf::Color(235, 235, 245));
            inp.setPosition(bx + 26.f, byp + 52.f); window.draw(inp);
            sf::Text hint; hint.setFont(hudFont);
            hint.setString("mot de passe  -  Entree: valider   Echap: annuler");
            hint.setCharacterSize(12); hint.setFillColor(sf::Color(170, 180, 200));
            hint.setPosition(bx + 18.f, byp + 92.f); window.draw(hint);
        }
        if (adminMode) {
            sf::Text h; h.setFont(hudFont);
            char hb[160];
            std::snprintf(hb, sizeof(hb),
                "MODE ADMIN  -  Z/fleches: voler   Espace: constellation   O/P: pose=%.1f   Q: quitter",
                mewIdlePose);
            h.setString(hb);
            h.setCharacterSize(16); h.setFillColor(sf::Color(150, 220, 255, 220));
            sf::FloatRect b = h.getLocalBounds();
            h.setPosition(ws.x / 2.f - b.width / 2.f, 42.f);
            window.draw(h);
        }
        if (charzMode) {
            sf::Text h; h.setFont(hudFont);
            bool onGround = charzPos.y <= CHARZ_GROUND_Y + 0.05f;
            const char* hint = onGround
                ? "CHARIZARD  -  fleches: marcher   Space: courir   Z: s'envoler   Q: quitter"
                : "CHARIZARD  -  fleches: voler   Space: turbo   Z/S: altitude   Q: quitter";
            h.setString(hint);
            h.setCharacterSize(16); h.setFillColor(sf::Color(255, 140, 60, 230));
            sf::FloatRect b = h.getLocalBounds();
            h.setPosition(ws.x / 2.f - b.width / 2.f, 42.f);
            window.draw(h);
        }
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        window.display();
    }
    return 0;
}
