import math

from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.explore import explore


def _go_to_tile(client: Client, index: int):
    if index == 0:
        return

    # la ligne r commence à l'index r² → r = floor(√index)
    r = int(math.sqrt(index))
    # décalage : négatif = gauche, 0 = centre, positif = droite
    col = index - r * (r + 1)

    # on construit la séquence de mouvements pour atteindre la tuile
    commands = ["Forward"] * r
    if col < 0:
        commands.append("Left")
        commands.extend(["Forward"] * abs(col))
    elif col > 0:
        commands.append("Right")
        commands.extend(["Forward"] * col)

    # on envoie tout le trajet en une seule salve (pipeline)
    client.send_batch(commands)


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

    # aucune food en vue : exploration intelligente pour couvrir la carte
    explore(client, player)
