import random

from network.client import Client
from logic.player import Player


def explore(client: Client, player: Player):
    # on incrémente le compteur de pas sans ressource trouvée
    player.explore_steps += 1

    commands = []
    # toutes les 3 avances, on tourne aléatoirement pour changer de direction
    # → évite de traverser la carte en ligne droite et de rater les côtés
    if player.explore_steps % 3 == 0:
        commands.append(random.choice(["Left", "Right"]))
    commands.append("Forward")

    # turn + forward en une seule salve
    client.send_batch(commands)
