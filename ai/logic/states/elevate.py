import random

from network.client import Client
from logic.player import Player, ELEVATION_REQ
from network.broadcast import encode

# pierres requises pour les incantations (food n'en fait pas partie)
STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def _wander(client: Client):
    # en attendant des coéquipiers on ne reste pas planté : on parcourt la carte.
    # on avance presque toujours et on ne tourne que rarement -> déplacement
    # visible (pas un joueur qui pivote sur place) tout en balayant la carte.
    if random.random() < 0.12:
        client.send(random.choice(["Left", "Right"]))
    client.send("Forward")


def _do_incantation(client: Client, player: Player):
    # le serveur vérifie les pierres SUR LA TUILE, pas dans l'inventaire :
    # on dépose donc les pierres requises avant de lancer l'incantation
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        for _ in range(req.get(stone, 0)):
            client.send(f"Set {stone}")

    # on lance l'incantation. Inutile d'attendre "Current level" ici : le serveur
    # l'envoie à TOUS les participants (sollicité ou non), et le client le met de
    # côté ; le brain applique le nouveau niveau au tour suivant. Attendre ici
    # bloquerait les participants qui n'ont pas eux-mêmes obtenu "Elevation underway".
    client.send("Incantation")
    


def elevate(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    # cas simple : 1 seul joueur nécessaire, on lance directement
    if needed == 1:
        _do_incantation(client, player)
        return

    # here_count = coéquipiers ayant activement confirmé leur arrivée (HERE),
    # +1 pour nous-même. Plus fiable que compter les "player" au Look (les
    # coéquipiers peuvent ne pas encore être arrivés sur la tuile).
    if player.here_count + 1 >= needed:
        # assez de joueurs confirmés → on lance et on remet le compteur à zéro
        player.here_count = 0
        _do_incantation(client, player)
    else:
        # pas assez → on appelle les coéquipiers du même niveau ET on se déplace
        # pour les croiser (sinon deux joueurs se broadcastent face à face sans
        # jamais se rejoindre — c'est la cause de l'immobilisme observé).
        # On n'émet l'appel que périodiquement : un broadcast à chaque tour noie
        # les coéquipiers sous les messages et les fait pivoter sur place.
        player.wander_tick += 1
        if player.wander_tick % 5 == 0:
            client.send(f"Broadcast {encode('NEED', str(player.level))}")
        _wander(client)
