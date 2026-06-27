import os
import sys
import time
import subprocess

from network.client import Client
from logic.player import Player

FORK_COOLDOWN_SEC        = 60.0  # cooldown des autres après réception d'un FORK
FORK_SELF_COOLDOWN_SEC   = 90.0  # cooldown du forkeur lui-même après do_fork
INITIAL_FORK_COOLDOWN    = 5.0   # cooldown court pour les forks d'initialisation
WANDER_FORK_THRESHOLD    = 30    # tours d'attente avant de créer un nouveau joueur
EMERGENCY_FORK_THRESHOLD = 400   # fork d'urgence si bloqué trop longtemps

# Seuil en tours sans partenaire avant de déclencher le mécanisme de rescue.
RESCUE_FORK_THRESHOLD = 80


def should_fork(client: Client, player: Player) -> bool:
    if player.level >= 8:
        return False
    if time.time() < player.fork_until:
        return False
    if player.wander_tick < WANDER_FORK_THRESHOLD:
        return False
    # La team n'est pas encore à 6 : on crée un nouveau joueur.
    # Le backoff aléatoire stagger l'éligibilité : le 1er joueur forke et son
    # broadcast FORK met les autres en cooldown (évite le thundering herd).
    if player.known_team_size < 6:
        return player.wander_tick >= WANDER_FORK_THRESHOLD + player.fork_backoff
    # Fork d'urgence : bloqué seul trop longtemps (deadlock inter-niveaux)
    return player.wander_tick >= EMERGENCY_FORK_THRESHOLD


def _spawn(client: Client, player: Player):
    """Envoie la commande Fork et lance le subprocess AI."""
    from network.broadcast import encode
    client.send(f"Broadcast {encode('FORK', f'{client.team}:{player.known_team_size}')}")
    client.send("Fork")
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


def do_fork(client: Client, player: Player):
    """Fork normal : crée 1 enfant et applique le cooldown long."""
    player.known_team_size += 1
    player.wander_tick = 0
    _spawn(client, player)
    player.fork_until = time.time() + FORK_SELF_COOLDOWN_SEC


def do_initial_fork(client: Client, player: Player):
    """Fork d'initialisation : cooldown court pour atteindre 6 rapidement."""
    player.known_team_size += 1
    _spawn(client, player)
    player.fork_until = time.time() + INITIAL_FORK_COOLDOWN


def rescue_fork(client: Client, player: Player, n: int):
    """Fork n enfants immédiatement pour débloquer un deadlock inter-niveaux.

    Les enfants vont s'entraider entre eux pour remonter jusqu'au niveau du
    joueur bloqué, lui fournir les partenaires dont il a besoin pour incanter,
    et lui permettre de rejoindre le reste de la team.
    """
    for _ in range(n):
        player.known_team_size += 1
        _spawn(client, player)
    player.fork_until = time.time() + FORK_SELF_COOLDOWN_SEC
