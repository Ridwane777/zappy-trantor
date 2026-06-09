import math

from network.client import Client
from logic.player import Player
from network.parser import parse_look


def _go_to_tile(client: Client, index: int):
    if index == 0:
        return

    # la ligne r commence à l'index r² → r = floor(√index)
    r = int(math.sqrt(index))
    # décalage : négatif = gauche, 0 = centre, positif = droite
    col = index - r * (r + 1)

    # on avance jusqu'à la bonne ligne
    for _ in range(r):
        client.send("Forward")

    # on se décale latéralement si nécessaire
    if col < 0:
        client.send("Left")
        for _ in range(abs(col)):
            client.send("Forward")
    elif col > 0:
        client.send("Right")
        for _ in range(col):
            client.send("Forward")


def survive(client: Client, player: Player):
    # on regarde autour pour voir les tuiles visibles et leur contenu
    raw = client.send("Look")
    tiles = parse_look(raw)

    # on cherche la tuile la plus proche qui contient de la food
    for index, tile in enumerate(tiles):
        if "food" in tile:
            # on s'y déplace puis on ramasse
            _go_to_tile(client, index)
            client.send("Take food")
            return

    # aucune food en vue : on avance et on repassera au prochain tour
    client.send("Forward")
