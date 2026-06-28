import math

from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.explore import explore


def _go_to_tile(client: Client, index: int):
    """Construit et envoie le trajet pour aller sur la case `index` du champ de vision.

    Rappel de la géométrie du "Look" : les cases sont numérotées en cône devant le
    joueur, ligne par ligne. La ligne r (r = 0 pour la case du joueur) contient
    2r+1 cases et commence à l'indice r². Son centre (juste devant le joueur, à r
    pas) est donc à l'indice r*(r+1).

        index :        0
                    1  2  3
                 4  5  6  7  8
        ligne r :   r²  ...  (r+1)²-1, centre = r*(r+1)

    Pour atteindre une case d'indice `index` :
      - r = partie entière de √index donne la LIGNE (donc le nombre de pas à
        avancer tout droit) ;
      - col = index - r*(r+1) donne le DÉCALAGE latéral par rapport au centre :
        négatif = à gauche, 0 = pile devant, positif = à droite.
    On avance donc de r cases, puis on tourne et on avance du décalage latéral.
    """
    if index == 0:
        return  # on est déjà sur la case visée

    r = int(math.sqrt(index))          # numéro de ligne = nombre de pas en avant
    col = index - r * (r + 1)          # décalage latéral (signé) par rapport au centre

    commands = ["Forward"] * r
    if col < 0:                        # case à gauche du centre
        commands.append("Left")
        commands.extend(["Forward"] * abs(col))
    elif col > 0:                      # case à droite du centre
        commands.append("Right")
        commands.extend(["Forward"] * col)

    # On envoie tout le trajet d'un coup (le serveur accepte jusqu'à 10 commandes
    # en file), c'est plus rapide que d'attendre la réponse après chaque pas.
    client.send_batch(commands)


def survive(client: Client, player: Player):
    """Trouve de la nourriture et la ramasse (action de dernier recours : manger).

    On regarde autour de soi, on va sur la case de food la plus proche, et on
    ramasse TOUTE la food qui s'y trouve (pas seulement une unité) pour reconstituer
    une vraie réserve. Si aucune food n'est en vue, on explore pour en chercher.
    """
    raw = client.send("Look")
    tiles = parse_look(raw)

    # On parcourt les cases du plus proche au plus loin et on s'arrête à la première
    # qui contient de la food.
    for index, tile in enumerate(tiles):
        count = tile.count("food")
        if count:
            _go_to_tile(client, index)
            for _ in range(count):
                client.send("Take food")
            return

    # Rien à manger en vue : on explore pour découvrir d'autres cases.
    explore(client, player)
