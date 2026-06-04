from network.client import Client
from logic.player import Player, ELEVATION_REQ
from network.broadcast import encode, decode


def elevate(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    if needed == 1:
        _do_incantation(client, player)
        return

    client.send(f"Broadcast {encode('NEED', str(player.level))}")

    response = client.send("Incantation")
    if "Elevation underway" in response:
        result = client.send("")
        if "level" in result:
            player.level += 1


def _do_incantation(client: Client, player: Player):
    response = client.send("Incantation")
    if "Elevation underway" in response:
        result = client.send("")
        if "level" in result:
            player.level += 1
