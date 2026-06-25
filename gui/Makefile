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
# -MMD -MP : dépendances (.d) pour recompiler les .cpp quand un header change.
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude -I$(HOME)/.local/include -pthread -MMD -MP
LDFLAGS  = -lGL -lGLEW -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -pthread

