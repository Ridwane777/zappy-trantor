import random

from network.client import Client
from logic.player import Player


def explore(client: Client, player: Player):
    """Déplacement d'exploration quand il n'y a rien d'utile en vue.

    But : couvrir le terrain pour finir par tomber sur de la food ou des pierres,
    sans traverser la carte en ligne droite (ce qui, sur une carte toroïdale,
    reviendrait à tourner en boucle et à rater les côtés).

    Méthode : on avance, et environ une fois sur trois on tourne d'abord d'un quart
    de tour dans une direction au hasard. Ce mélange "avance + virages
    occasionnels" donne une trajectoire en zig-zag qui balaie mieux la zone.
    """
    player.explore_steps += 1

    commands = []
    # Tous les 3 pas, on change de cap (gauche ou droite au hasard).
    if player.explore_steps % 3 == 0:
        commands.append(random.choice(["Left", "Right"]))
    commands.append("Forward")

    # Virage éventuel + avance, envoyés en une seule salve.
    client.send_batch(commands)
