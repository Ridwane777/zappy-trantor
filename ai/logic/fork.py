import os
import sys
import subprocess

from network.client import Client
from logic.player import Player

# l'IA ne va chercher de la food que sous 12 (FOOD_THRESHOLD), donc elle stocke
# rarement beaucoup : on fork dès qu'on a un petit coussin au-dessus de la survie
FORK_FOOD_THRESHOLD = 16
# nombre de tours à attendre avant de pouvoir forker à nouveau
FORK_COOLDOWN = 30


def should_fork(player: Player) -> bool:
    # inutile de forker si on a déjà gagné
    if player.level >= 8:
        return False
    # on ne fork pas si on risque de mourir de faim
    if player.inventory["food"] < FORK_FOOD_THRESHOLD:
        return False
    # on attend que le cooldown soit écoulé
    return player.fork_cooldown == 0


def do_fork(client: Client, player: Player):
    # on pond un œuf sur la tuile actuelle
    client.send("Fork")

    # on arme le cooldown pour ne pas forker à nouveau tout de suite
    player.fork_cooldown = FORK_COOLDOWN

    # on lance un nouvel AI qui va éclore à la position de l'œuf
    entry = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "zappy_ai")
    subprocess.Popen(
        [sys.executable, entry, "-p", str(client.port), "-n", client.team, "-h", client.host],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
