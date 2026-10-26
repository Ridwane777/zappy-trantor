#!/usr/bin/env bash
# Installs the header-only libraries (nlohmann/json, tinygltf) as a
# real dependency: in an include path, NOT in the repo. Fetched from
# their public repository, then placed in $PREFIX/include.
#
#   ./setup_libs.sh                       # -> ~/.local/include (default, no sudo)
#   PREFIX=/usr/local sudo ./setup_libs.sh # -> system install (like SFML/GLM)
#
# The Makefile calls it automatically on the first `make`, nothing to do by hand.
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"
INCDIR="$PREFIX/include"
BASE="https://raw.githubusercontent.com/Knight-coder-19/gltf-json-header-libs/master"
FILES=(json.hpp tiny_gltf.h tinygltf_json.hpp)

mkdir -p "$INCDIR"
for f in "${FILES[@]}"; do
    [ -s "$INCDIR/$f" ] && { echo "  $f already installed ($INCDIR)"; continue; }
    echo "  installing $f -> $INCDIR ..."
    tmp="$(mktemp)"
    curl -fsSL "$BASE/$f" -o "$tmp"
    install -m 644 "$tmp" "$INCDIR/$f"
    rm -f "$tmp"
done
echo "OK - third-party headers installed in $INCDIR (like a system lib)"
