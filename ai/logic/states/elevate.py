import random

from network.client import Client
from logic.player import Player, ELEVATION_REQ
from network.broadcast import encode
from logic.fork import should_fork, do_fork

STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def _wander(client: Client):
    if random.random() < 0.12:
        client.send(random.choice(["Left", "Right"]))
    client.send("Forward")


def _do_incantation(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        for _ in range(req.get(stone, 0)):
            client.send(f"Set {stone}")
    client.send("Incantation")


def elevate(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    if needed == 1:
        _do_incantation(client, player)
        return

    if player.here_count + 1 >= needed:
        player.here_count = 0
        player.wander_tick = 0
        _do_incantation(client, player)
    else:
        player.wander_tick += 1
        if player.wander_tick % 5 == 0:
            client.send(
                f"Broadcast {encode('NEED', f'{client.team}:{player.level}')}"
            )
        if should_fork(client, player):
            do_fork(client, player)
        _wander(client)
