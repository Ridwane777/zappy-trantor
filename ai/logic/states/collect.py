from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.states.survive import _go_to_tile
from logic.explore import explore


def collect(client: Client, player: Player):
    # STOCKPILE-AHEAD : on collecte d'avance le CUMUL des pierres de TOUS les
    # niveaux restants. Une fois stocké, le joueur n'a plus jamais à repartir
    # collecter entre deux niveaux → la cohorte co-localisée peut enchaîner les
    # incantations sans se redisperser.
    missing = player.missing_stockpile()

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
            # bonus opportuniste : tant qu'on est déjà sur place, on ramasse
            # aussi la food qui s'y trouve (sinon elle ne monte jamais au-delà
            # du strict seuil de survie, et le fork ne se déclenche jamais)
            for _ in range(tile.count("food")):
                client.send("Take food")
            return

    # aucune ressource en vue : exploration intelligente pour couvrir la carte
    explore(client, player)
