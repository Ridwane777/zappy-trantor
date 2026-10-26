#!/usr/bin/env bash
# Runs the full demo: server + 2 teams + GUI.
# Only ONE AI is started per team: it is the team's QUEEN. She only survives
# and forks to maintain the headcount (rebuilds below 7, cap 9); her
# workers level up and cooperate across teams (cross-team incantations per
# level). Press START after the intro and the GUI joins the game.
#
# Usage: ./run_demo.sh [MOVE_SEC] [PORT]
#   MOVE_SEC  duration of one move in seconds  (default 0.5 -> ~2 tiles/s;
#             higher = slower)
#   PORT      server port                       (default 4242)
#
# Run it from your own terminal so the window stays open
# until you close it. Ctrl-C stops everything cleanly.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AI_DELAY="${1:-0.5}"
PORT="${2:-4242}"
WIDTH=20
HEIGHT=20
PLAYERS_PER_TEAM=9
TEAMS=(team1 team2)

pids=()
cleanup() {
    echo
    echo "[run_demo] stopping..."
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[run_demo] server port $PORT - ${#TEAMS[@]} teams x $PLAYERS_PER_TEAM players (map ${WIDTH}x${HEIGHT})"
"$ROOT/server/zappy_server" -p "$PORT" -x "$WIDTH" -y "$HEIGHT" \
    -n "${TEAMS[@]}" -c "$PLAYERS_PER_TEAM" -f 100 &
pids+=($!)
sleep 1

echo "[run_demo] AI: 1 client per team (the fill chain populates up to -c=$PLAYERS_PER_TEAM; delay=${AI_DELAY}s/move)"
for team in "${TEAMS[@]}"; do
    ( cd "$ROOT/ai" && ZAPPY_AI_DELAY="$AI_DELAY" \
        python3 zappy_ai -p "$PORT" -n "$team" -h localhost ) &
    pids+=($!)
done
sleep 1

# No ZAPPY_AUTOSTART: the GUI plays the intro, then START joins the game.
echo "[run_demo] GUI - intro then START to join 127.0.0.1:$PORT (close the window / Ctrl-C to quit)"
cd "$ROOT/gui"
./zappy_gui -p "$PORT" -h localhost
