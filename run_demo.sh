#!/usr/bin/env bash
# Lance la démo complète : serveur + 6 équipes de 5 IA + GUI.
# Les IA se connectent dès le départ et jouent pendant l'intro ; quand tu
# appuies sur START après l'intro, la GUI rejoint la partie déjà en cours.
#
# Usage : ./run_demo.sh [MOVE_SEC] [PORT]
#   MOVE_SEC  durée d'un déplacement en secondes  (défaut 0.5 -> ~2 tuiles/s ;
#             plus haut = plus lent)
#   PORT      port du serveur                      (défaut 4242)
#
# À lancer depuis ton propre terminal (pas via l'agent) pour que la fenêtre
# reste ouverte tant que tu ne la fermes pas. Ctrl-C arrête tout proprement.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AI_DELAY="${1:-0.5}"
PORT="${2:-4242}"
WIDTH=20
HEIGHT=20
PLAYERS_PER_TEAM=5
TEAMS=(team1 team2 team3 team4 team5 team6)

pids=()
cleanup() {
    echo
    echo "[run_demo] arrêt..."
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[run_demo] serveur port $PORT — ${#TEAMS[@]} équipes x $PLAYERS_PER_TEAM joueurs (carte ${WIDTH}x${HEIGHT})"
"$ROOT/server/zappy_server" -p "$PORT" -x "$WIDTH" -y "$HEIGHT" \
    -n "${TEAMS[@]}" -c "$PLAYERS_PER_TEAM" -f 100 &
pids+=($!)
sleep 1

echo "[run_demo] IA : $((${#TEAMS[@]} * PLAYERS_PER_TEAM)) clients (délai=${AI_DELAY}s/déplacement)"
for team in "${TEAMS[@]}"; do
    for _ in $(seq "$PLAYERS_PER_TEAM"); do
        ( cd "$ROOT/ai" && ZAPPY_AI_DELAY="$AI_DELAY" \
            python3 zappy_ai -p "$PORT" -n "$team" -h localhost \
            --team-size "$PLAYERS_PER_TEAM" ) &
        pids+=($!)
    done
done
sleep 1

# Pas de ZAPPY_AUTOSTART : la GUI joue l'intro, puis START rejoint la partie.
echo "[run_demo] GUI — intro puis START pour rejoindre 127.0.0.1:$PORT (ferme la fenêtre / Ctrl-C pour quitter)"
cd "$ROOT/gui"
./zappy_gui -p "$PORT" -h localhost
