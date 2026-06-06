from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.states.survive import _go_to_tile


def collect(client: Client, player: Player):
    # on récupère ce qui manque pour le prochain niveau
    missing = player.missing_for_elevation()

    # on regarde autour pour voir les tuiles visibles et leur contenu
    raw = client.send("Look")
    tiles = parse_look(raw)

    # pour chaque ressource manquante, on cherche une tuile qui la contient
    for resource in missing:
        for index, tile in enumerate(tiles):
            if resource in tile:
                # on s'y déplace et on ramasse
                _go_to_tile(client, index)
                client.send(f"Take {resource}")
                return

    # aucune ressource en vue : on avance et on repassera au prochain tour
    client.send("Forward")
