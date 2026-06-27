import math

from network.client import Client
from network.parser import parse_look
from logic.player import Player, ELEVATION_REQ
from network.broadcast import encode
from logic.fork import should_fork, do_fork, rescue_fork, RESCUE_FORK_THRESHOLD

STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def _step_toward_tile(client: Client, tile_idx: int):
    """Avance d'un seul pas vers une tuile visible (convergence locale de
    proximité, en complément du homing par broadcast vers l'ancre)."""
    if tile_idx <= 0:
        return
    r = int(math.sqrt(tile_idx))
    col = tile_idx - r * (r + 1)
    if col < 0:
        client.send("Left")
        client.send("Forward")
    elif col > 0:
        client.send("Right")
        client.send("Forward")
    else:
        client.send("Forward")


def _do_incantation(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        for _ in range(req.get(stone, 0)):
            client.send(f"Set {stone}")
    result = client.send_incantation()
    if result.startswith("Current level"):
        try:
            player.level = int(result.split(":")[1].strip())
            client.send(f"Broadcast {encode('RESUME', f'{client.team}:{player.level}')}")
        except (IndexError, ValueError):
            pass


def elevate(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    if needed == 1:
        _do_incantation(client, player)
        return

    raw = client.send("Look")
    tiles = parse_look(raw)
    players_here = max(0, tiles[0].count("player") - 1) if tiles else 0
    player.partners_here = players_here

    if players_here + 1 >= needed:
        player.here_count = 0
        player.wander_tick = 0
        _do_incantation(client, player)
        return

    player.is_elevating_wait = True
    player.wander_tick += 1

    # --- Convergence locale de proximité (Look) ---
    # Si un groupe plus grand est visible juste à côté, on s'en rapproche d'un
    # pas. Complète le homing par broadcast (ancre) pour les rapprochements fins.
    best_idx = -1
    best_cnt = 0
    for i in range(1, len(tiles)):
        cnt = tiles[i].count("player")
        if cnt > best_cnt:
            best_cnt = cnt
            best_idx = i
    if best_idx >= 0 and best_cnt > players_here:
        _step_toward_tile(client, best_idx)

    # --- Rassemblement par ancre élue (Broadcast) ---
    # On diffuse NEED:team:level:weight. L'ancre = le joueur de plus haut weight
    # parmi ceux du même niveau ; elle reste immobile, les autres convergent vers
    # elle (voir brain._handle_broadcasts). Broadcasts globaux sur ce serveur.
    if player.wander_tick % 4 == 0:
        client.send(
            f"Broadcast {encode('NEED', f'{client.team}:{player.level}:{player.rally_weight}')}"
        )

    # Rescue : si seul depuis trop longtemps → STUCK + fork des enfants.
    if players_here == 0 and player.wander_tick >= RESCUE_FORK_THRESHOLD:
        if player.wander_tick % 20 == 0:
            client.send(f"Broadcast {encode('STUCK', f'{client.team}:{player.level}')}")
        if player.wander_tick == RESCUE_FORK_THRESHOLD:
            rescue_fork(client, player, needed)

    if should_fork(client, player):
        do_fork(client, player)
