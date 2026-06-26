import os
import sys
import time
import subprocess

from network.client import Client
from logic.player import Player

FORK_COOLDOWN_SEC      = 10.0   # cooldown des autres après réception d'un FORK
FORK_SELF_COOLDOWN_SEC = 20.0   # cooldown du forkeur lui-même
WANDER_FORK_THRESHOLD  = 30     # tours d'attente avant de créer un nouveau joueur


def should_fork(client: Client, player: Player) -> bool:
    if player.level >= 8:
        return False
    if time.time() < player.fork_until:
        return False
    # plafond absolu : le jeu ne nécessite jamais plus de 6 joueurs par team
    if player.known_team_size >= 6:
        return False
    # on attend assez longtemps : si un coéquipier allait répondre,
    # il aurait déjà répondu aux broadcasts NEED émis toutes les 5 itérations
    if player.wander_tick < WANDER_FORK_THRESHOLD:
        return False
    return True


def do_fork(client: Client, player: Player):
    from network.broadcast import encode

    player.known_team_size += 1
    player.wander_tick = 0

    # broadcast avec le nom de l'équipe pour que seuls les coéquipiers
    # mettent à jour leur compteur (évite la contamination cross-team)
    client.send(f"Broadcast {encode('FORK', f'{client.team}:{player.known_team_size}')}")
    client.send("Fork")
    player.fork_until = time.time() + FORK_SELF_COOLDOWN_SEC

    # le subprocess hérite de known_team_size via --team-size
    # → il ne forkera pas immédiatement si la team est déjà assez grande
    entry = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "zappy_ai")
    subprocess.Popen(
        [
            sys.executable, entry,
            "-p", str(client.port),
            "-n", client.team,
            "-h", client.host,
            "--team-size", str(player.known_team_size),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
