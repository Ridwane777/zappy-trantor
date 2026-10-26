#!/usr/bin/env bash
# Test the Fork / procreation feature:
# - server starts with -c 1 (one slot per team = one starting player)
# - fork threshold is lowered via env so AIs fork as soon as they have a
#   small food buffer, instead of the normal game threshold
# - watch the player count grow in the GUI HUD: 1 -> 2 -> 4 ...
#
# Usage: ./test_fork.sh [PORT]
#   PORT  (default 4242)
#
# Run from your own terminal. Ctrl-C stops everything.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="${1:-4343}"
TEAMS=(team1 team2 team3 team4)

pids=()
cleanup() {
    echo
    echo "[test_fork] stopping..."
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[test_fork] server port $PORT - ${#TEAMS[@]} teams x 1 player (map 10x10, f=50)"
"$ROOT/server/zappy_server" -p "$PORT" -x 10 -y 10 \
    -n "${TEAMS[@]}" -c 1 -f 50 &
pids+=($!)
sleep 1

# FORK_FOOD_THRESHOLD=5 : fork as soon as the AI has a tiny food buffer
# ZAPPY_AI_DELAY=0.2 : move fast so food accumulates quickly
echo "[test_fork] starting 1 AI per team (fork threshold=5, delay=0.2s)"
for team in "${TEAMS[@]}"; do
    ( cd "$ROOT/ai" && ZAPPY_AI_DELAY=0.2 FORK_FOOD_THRESHOLD=5 \
        python3 zappy_ai -p "$PORT" -n "$team" -h localhost ) &
    pids+=($!)
done
sleep 1

echo "[test_fork] GUI — live observer on 127.0.0.1:$PORT"
echo "            Watch the HUD: players should grow from 2 -> 4 -> 8 ..."
cd "$ROOT/gui"
./zappy_gui -p "$PORT" -h localhost
