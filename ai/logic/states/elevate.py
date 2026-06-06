from network.client import Client
from logic.player import Player, ELEVATION_REQ
from network.parser import parse_look
from network.broadcast import encode


def _count_players_on_tile(client: Client) -> int:
    # on regarde autour et on compte les "player" sur la tuile 0 (ma tuile)
    raw = client.send("Look")
    tiles = parse_look(raw)
    return tiles[0].count("player")


def _do_incantation(client: Client, player: Player):
    # on envoie la commande — le serveur répond "Elevation underway" ou "ko"
    response = client.send("Incantation")

    if "Elevation underway" in response:
        # on attend le 2ème message du serveur : "Current level: k"
        result = client.receive()
        if "Current level" in result:
            player.level += 1


def elevate(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    # cas simple : 1 seul joueur nécessaire, on lance directement
    if needed == 1:
        _do_incantation(client, player)
        return

    # on vérifie combien de joueurs sont déjà sur notre tuile
    present = _count_players_on_tile(client)

    if present >= needed:
        # assez de joueurs → on lance l'incantation
        _do_incantation(client, player)
    else:
        # pas assez → on broadcast pour appeler les coéquipiers du même niveau
        client.send(f"Broadcast {encode('NEED', str(player.level))}")
