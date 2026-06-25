from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.states.survive import _go_to_tile
from logic.explore import explore


def collect(client: Client, player: Player):
    # on récupère ce qui manque pour le prochain niveau
    missing = player.missing_for_elevation()

    # on regarde autour pour voir les tuiles visibles et leur contenu
    raw = client.send("Look")
    tiles = parse_look(raw)

    # on cherche la première tuile qui contient au moins une ressource manquante
    for index, tile in enumerate(tiles):
        resources_here = [r for r in missing if r in tile]
        if resources_here:
            # on se déplace une seule fois vers cette tuile
            _go_to_tile(client, index)
            # on ramasse toutes les ressources manquantes présentes sur cette tuile
            for resource in resources_here:
                client.send(f"Take {resource}")
            return

    # aucune ressource en vue : exploration intelligente pour couvrir la carte
    explore(client, player)
