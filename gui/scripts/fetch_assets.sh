#!/usr/bin/env bash
# Downloads the GUI assets (models, textures, sounds, fonts) from the GitHub release
# and extracts them into gui/assets/. They are kept out of git because of their size.
# Credits and licenses: gui/ASSETS.md.
set -euo pipefail

RELEASE_URL="${ZAPPY_ASSETS_URL:-https://github.com/Ridwane777/zappy-trantor/releases/download/assets-v1}"
ARCHIVE="zappy-assets.zip"
GUI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -f "$GUI_DIR/assets/ASSETS.md" ] && [ "${1:-}" != "--force" ]; then
    echo "assets already installed in $GUI_DIR/assets (use --force to download again)"
    exit 0
fi

echo "downloading $ARCHIVE (about 750 MB)..."
curl -fL --progress-bar -o "$TMP/$ARCHIVE" "$RELEASE_URL/$ARCHIVE"
curl -fsSL -o "$TMP/$ARCHIVE.sha256" "$RELEASE_URL/$ARCHIVE.sha256"

echo "checking SHA-256..."
(cd "$TMP" && sha256sum -c "$ARCHIVE.sha256")

echo "extracting into $GUI_DIR/assets..."
unzip -q -o "$TMP/$ARCHIVE" -d "$GUI_DIR"
echo "done"
