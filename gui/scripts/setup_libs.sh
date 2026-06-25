#!/usr/bin/env bash
# Installe les bibliothèques header-only (nlohmann/json, tinygltf) comme une
# vraie dépendance : dans un include path, PAS dans le repo. Récupérées depuis
# leur dépôt public, puis posées dans $PREFIX/include.
#
#   ./setup_libs.sh                       # -> ~/.local/include (défaut, sans sudo)
#   PREFIX=/usr/local sudo ./setup_libs.sh # -> install système (comme SFML/GLM)
#
# Le Makefile l'appelle tout seul au 1er `make`, donc rien à faire à la main.
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"
INCDIR="$PREFIX/include"
