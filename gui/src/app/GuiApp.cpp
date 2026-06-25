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

    // ── load skinned characters (Character/CharState in app/Character.hpp) ──────
    std::vector<Character> characters;
    const float CHAR_HEIGHT = 0.01f;

    // ── incantation ritual effects (type + draw in app/IncantationFx) ──────────
    std::vector<IncantationEffect> incantations;
    float incantCooldown = 25.f;  // first demo fires at 25s

    const std::string CHAR_DIR   = "assets/characters/";
    const std::string FEMALE_DIR = CHAR_DIR + "female/Meshy_AI_Huntress_of_the_Wild_biped_Animation_";
    const std::string MALE_DIR   = CHAR_DIR + "male/Meshy_AI_Ironhorn_Wanderer_biped_Animation_";

    advanceLoadingTo(window, hudFont, 0.10f, "Loading characters...");

    // ── female: Huntress — Idle_6, then laying down after 10s ────────────────
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

    // ── male: Barefoot Survivor — Idle_11 ─────────────────────────────────────
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

    // ── player tiers: one character per 2 levels (1-2,3-4,5-6,7-8) ─────────────
    // anim slots: 0 idle, 1 walk, 2 run, 3 turn-left, 4 turn-right (run/turn
    // optional — only present for the Meshy "biped" exports).
    PlayerTier tiers[4];
    const char* tierDir[4] = { "woodsprite", "nomad", "nyxara", "huntress" };
    // The skinned mesh is far larger than its bind-pose bounds (armature scale),
    // so empirical per-tier factors are used; globally overridable via env.
    float globalPScale = std::getenv("ZAPPY_PSCALE")
                       ? (float)std::atof(std::getenv("ZAPPY_PSCALE")) : 0.05f / 6.f;
    for (int t = 0; t < 4; ++t) {
        std::string dir = std::string("assets/characters/") + tierDir[t] + "/";
        try {
            tiers[t].mesh = SkinnedMesh::load(dir + "idle.glb");
            tiers[t].mesh.loadAnimFrom(dir + "walk.glb");
            // optional extra anims — load only if the tier actually has them
            // (presence checked via the RAM store, not a local file)
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

    // Decorative demo characters (Huntress/Ironhorn at island centre) are not
    // server-driven — drop them so only live players are shown.
    characters.clear();

    // ── load map ──────────────────────────────────────────────────────────────
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

    // Index map tiles by (x,y) so live server resources can be written in O(1).
    std::unordered_map<long long, std::size_t> tileIndex;
    for (std::size_t k = 0; k < map.tiles.size(); ++k)
        tileIndex[(long long)map.tiles[k].y * 100000 + map.tiles[k].x] = k;

    // ── connect to the server as a GRAPHIC client ─────────────────────────────
    GuiClient netClient;
    bool netConnected = false;
    {
        std::string err;
        netConnected = netClient.connect(menuCfg.host, menuCfg.port, err);
        if (netConnected)
            std::cout << "[net] connected to " << menuCfg.host << ":"
                      << menuCfg.port << "\n";
        else
            std::cerr << "[net] connect failed: " << err
                      << " - running as offline viewer\n";
    }

    // (font already loaded above for the startup menu)

    // per-player animation state (movement/turn detection from discrete updates)
    std::unordered_map<int, PlayerAnim> playerAnims;

    // evolution sparkles + tree motes (Spark defined in app/Particle.hpp)
    std::vector<Spark> sparks;
    std::vector<Spark> treeMotes;          // uncanny tree-of-life energy particles
    const float EVO_DUR = 2.0f;    // length of the evolution flash (matches level-up sfx)

    // ── local demo mode: ZAPPY_TESTPLAYERS=N spawns N dummy players that grow
    // from level 1 to 8, one level every 20s (loops). Renders without a server.
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

    // ── camera (isometric-ish: 45° yaw, 50° pitch) ───────────────────────────
    float cx = map.w * TILE * 0.5f;
    float cz = map.h * TILE * 0.5f;
    float camYaw   = 45.f  * 3.14159f / 180.f;
    float camPitch = 35.f  * 3.14159f / 180.f;
    int   nightOverride = 0;   // 0=auto cycle, 1=force night, 2=force day (toggle with N)
    float camDist  = std::max(map.w, map.h) * TILE * 1.1f;
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

    // ── admin mode: press A, type "admin1234", fly Mew in 3rd person, Q to quit ──
    SkinnedMesh mewMesh;
    bool  haveMew = false;
    try {
        mewMesh = SkinnedMesh::load("assets/characters/mew/scene.gltf");
        mewMesh.loadTexture("assets/characters/mew/textures/blinn1_baseColor.jpeg");
        haveMew = true;
    } catch (std::exception& e) { std::cerr << "Mew load: " << e.what() << "\n"; }

    // ── night-cycle moon (textured, self-lit, fades in at night) ──
    SkinnedMesh moonMesh;
    bool haveMoon = false;
    try {
        moonMesh = SkinnedMesh::load("assets/models/moon/scene.gltf");
        moonMesh.loadTexture("assets/models/moon/textures/moon_baseColor.png");
        haveMoon = true;
    } catch (std::exception& e) { std::cerr << "Moon load: " << e.what() << "\n"; }

    // lowest Mew may descend to — keeps his whole body above the map surface
    const float MEW_FLOOR = std::getenv("ZAPPY_MEW_FLOOR")
                          ? (float)std::atof(std::getenv("ZAPPY_MEW_FLOOR")) : 1.0f;
    const float MEW_SCALE = std::getenv("ZAPPY_MEW_SCALE")
                          ? (float)std::atof(std::getenv("ZAPPY_MEW_SCALE")) : 0.27f;
    // pose figée à l'arrêt : une frame de vol (pas la frame 0 qui est verticale)
    float       mewIdlePose = std::getenv("ZAPPY_MEW_POSE")
                          ? (float)std::atof(std::getenv("ZAPPY_MEW_POSE")) : 2.0f;
    const float mewAnimDur = (haveMew && mewMesh.animCount() > 0)
                          ? mewMesh.animDuration(0) : 11.f;
    bool        adminPrompt = false;     // password input box open
    bool        adminEatNext = false;    // ignore the 'a' that opened the box
    bool        adminMode   = false;     // currently flying Mew
    std::string adminInput;
    glm::vec3   mewPos(cx, 6.f, cz);
    float       mewYaw = 0.f, mewAnimTime = mewIdlePose;
    bool        mewMoving = false;
    float       mewFlyTimer = 0.f;   // throttles the whoosh while Mew is moving
    float       mewMeowTimer = 10.f; // random ambient meow every ~10 s in admin mode

    // ── night sky: scattered stars + Big Dipper + Mew's purple-energy channel ──
    Constellation constellation;
    // third-person camera offset around Mew (mouse look), reset with R
    float admCamYaw = 0.f, admCamPitch = 0.32f;

    // ── in-game audio: ambient bed + event SFX (missing clips are silent) ───────
    AudioManager gameAudio;
    gameAudio.init("assets/audio");
    if (netConnected) gameAudio.playMusic("game_music", true, 35.f);
    unsigned long lastLogTotal = 0;   // for draining new action events into SFX
    float pickupCooldown = 0.f;       // throttles the (frequent) grab blip

    sf::Clock clock;
    float elapsed  = 0.f;

    // ── Tree of Life pulse state ──────────────────────────────────────────────
    const glm::vec3 TREE_POS     = {22*TILE, 0.f, 22*TILE};
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

    // ── 2D overlay (HUD line, team/player sidebar, inventory popup, feed) ──────
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

        // ── drain new action events into game SFX (levelup/hatch handled at draw) ─
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

        // ── live server state: drain socket, mirror tile resources onto the map ─
        if (netConnected) {
            netClient.poll();
            netConnected = netClient.connected();
            const GuiState& gs = netClient.state();
            if (gs.sizeKnown) {
                // Spread the server grid across the island's central LAND area
                // (the raw 0..W corner is mostly water), accumulating collisions.
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

        // ── demo mode: hatch from egg (lvl 0) then grow to 8 (loops) ──────────
        // seconds per phase configurable via ZAPPY_TESTSEC (default 20)
        if (testMode) {
            static float phaseSec = std::getenv("ZAPPY_TESTSEC")
                                  ? (float)std::atof(std::getenv("ZAPPY_TESTSEC")) : 1.f;
            testTimer += dt;
            int lvl = ((int)(testTimer / phaseSec)) % 9;   // 0(egg),1,...,8, loop
            for (auto& p : testPlayers) p.level = lvl;
        }

        // ── pulse state machine ───────────────────────────────────────────────
        pulseTimer += dt;
        switch (pulseState) {
        case PulseState::IDLE: {
            chargeT = 0.f;
            // When connected, charge during the final CHARGE_DURATION seconds before
            // the resource reset so the tree "pulses" right as it fires. Offline,
            // fall back to the ambient self-timed cycle.
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
        // foliage glow follows the charge, then fades out gently after release
        // (instead of snapping off the instant the energy is released)
        if (pulseState == PulseState::CHARGING)
            treeGlow = chargeT;
        else
            treeGlow = std::max(0.f, treeGlow - dt * 0.5f);   // ~2s fade-out

        for (auto& ch : characters)
            if (ch.auraTimer > 0.f) ch.auraTimer -= dt;

        // ── demo incantation trigger ──────────────────────────────────────────
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

        // ── events ────────────────────────────────────────────────────────────
        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed)
                window.close();

            // ── admin password box: A opens it, type, Enter validates, Esc cancels ──
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
                        if (adminInput == "admin1234") {                 // déverrouillage
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
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::A && !adminMode) {
                adminPrompt = true; adminInput.clear(); adminEatNext = true;  // ouvrir le prompt
                continue;
            }
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Q && adminMode) {
                adminMode = false;                                       // quitter le mode admin
                gameAudio.stopLoop();                                    // couper le bourdon de vol
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
                if (adminMode) {                       // reset the 3rd-person view
                    admCamYaw = 0.f; admCamPitch = 0.32f;
                } else {
                    camYaw = 45.f * 3.14159f / 180.f;
                    camPitch = 35.f * 3.14159f / 180.f;
                    camDist = std::max(map.w, map.h) * TILE * 1.1f;
                }
            }
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Tab)
                hud.toggle();
            // N: cycle day/night override (auto -> force night -> force day -> auto)
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::N)
                nightOverride = (nightOverride + 1) % 3;

            if (ev.type == sf::Event::MouseButtonPressed)
                gameAudio.play("click", 65.f);                       // son à chaque clic

            if (ev.type == sf::Event::MouseButtonPressed &&
                ev.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2i mp = sf::Mouse::getPosition(window);
                if (!hud.handleClick(mp)) {                          // empty space → orbit
                    dragging = true;
                    lastMouse = mp;
                }
            }
            if (ev.type == sf::Event::MouseButtonReleased)
                dragging = false;

            if (ev.type == sf::Event::MouseMoved && dragging) {
                sf::Vector2i cur = sf::Mouse::getPosition(window);
                if (adminMode) {                              // look around Mew
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
