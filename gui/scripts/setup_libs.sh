#!/usr/bin/env bash
# Installe les bibliothèques header-only (nlohmann/json, tinygltf) comme une
# vraie dépendance : dans un include path, PAS dans le repo. Récupérées depuis
# leur dépôt public, puis posées dans $PREFIX/include.
#
#   ./setup_libs.sh                       # -> ~/.local/include (défaut, sans sudo)
