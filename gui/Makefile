# Zappy GUI — isometric 3D viewer + server (GRAPHIC) client.
#
# Targets:
#   all     : build zappy_gui (auto-installs header-only deps on first run)
#   deps    : install third-party header libs (scripts/setup_libs.sh)
#   clean   : remove object files
#   fclean  : clean + remove the binary
#   re      : fclean + all
#
# Assets are NOT committed: the GUI streams them from the CDN at runtime.

CXX      = g++
# Les libs header-only (json, tinygltf) sont installées comme une dépendance
# dans le include path utilisateur (~/.local/include) — voir setup_libs.sh —
# et PAS versionnées dans le repo. -I système (/usr/local/include) marche aussi.
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude -I$(HOME)/.local/include -pthread
LDFLAGS  = -lGL -lGLEW -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lX11 -pthread

NAME = zappy_gui

SRCS = src/render/Shader.cpp \
       src/render/Mesh.cpp \
       src/render/ObjLoader.cpp \
       src/render/SkinnedMesh.cpp \
       src/render/Camera.cpp \
       src/render/TexturedMesh.cpp \
       src/net/GuiClient.cpp \
       src/menu/MenuScene.cpp \
       src/audio/AudioManager.cpp \
       src/hud/HudOverlay.cpp \
       src/world/World.cpp \
       src/app/SkyDayNight.cpp \
       src/app/Constellation.cpp \
       src/app/TreeMotes.cpp \
       src/app/IncantationFx.cpp \
       src/app/WorldRender.cpp \
       src/app/TileDebug.cpp \
       src/app/Islands.cpp \
       src/app/CharacterRender.cpp \
       src/app/Wildlife.cpp \
       src/app/PlayerRender.cpp \
       src/app/BillMesh.cpp \
       src/app/SettingsOverlay.cpp \
       src/app/OutroScene.cpp \
       src/app/Fullscreen.cpp \
       src/app/GuiApp.cpp \
       src/main.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Header-only deps installed like a real library (into ~/.local/include), never
# bundled in the repo. The stamp is one of the installed headers; if it's
# missing, `make` runs setup_libs.sh once before compiling (zero manual setup).
DEPS_STAMP := $(HOME)/.local/include/tiny_gltf.h

deps:
	@./scripts/setup_libs.sh

$(DEPS_STAMP):
	@./scripts/setup_libs.sh

# objects need the deps installed (installed once, order-only prerequisite)
$(OBJS): | $(DEPS_STAMP)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all deps clean fclean re
