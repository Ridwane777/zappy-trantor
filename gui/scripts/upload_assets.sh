#!/usr/bin/env bash
# Pousse gui/assets/ vers le bucket R2 (CDN). Lit les identifiants dans .env.local.
set -euo pipefail
cd "$(dirname "$0")/.."          # -> gui/ (le script vit dans gui/scripts/)
set -a; . ./.env.local; set +a
RC="${RCLONE:-/home/knight-coder/bin/rclone}"
"$RC" copy assets/ ":s3:$R2_BUCKET" \
