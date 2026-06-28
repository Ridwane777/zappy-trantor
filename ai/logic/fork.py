import os
import sys
import time
import subprocess

from network.client import Client

# Effectif d'équipe visé (PAR ÉQUIPE). On force 9 quel que soit le -c du serveur :
# redondance pour tenir les quorums (4 puis 6) des hauts niveaux malgré les morts.
TEAM_TARGET = 9

# Cooldown court entre deux créations de joueur (le temps que l'œuf forké éclose
# et que l'enfant se connecte, pour ne pas spammer pendant la latence du Fork).
BUILD_FORK_COOLDOWN = 2.0


def _spawn(client: Client) -> subprocess.Popen:
    """Lance un nouveau process zappy_ai et renvoie son handle (pour supervision).

    L'enfant reçoit la variable d'env ZAPPY_CHILD=1 : c'est un simple RÔLE de
    lancement (réinforcement), pas une donnée de jeu. Il NE recrute pas (seul le
    recruteur gère l'effectif) ; il joue normalement. Aucune info de jeu (taille
    d'équipe, etc.) n'est transmise → l'interdiction de communiquer hors-jeu est
    respectée. À la connexion, le serveur lui attribue un slot initial libre, sinon
    fait éclore un œuf (Fork) de l'équipe.
    """
    entry = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "zappy_ai")
    env = dict(os.environ, ZAPPY_CHILD="1")
    return subprocess.Popen(
        [
            sys.executable, entry,
            "-p", str(client.port),
            "-n", client.team,
            "-h", client.host,
        ],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


class TeamManager:
    """Construit et MAINTIENT l'équipe à TEAM_TARGET joueurs (par équipe).

    Un seul process est « recruteur » (celui lancé à la main, sans ZAPPY_CHILD) ;
    les autres (lancés par _spawn, avec ZAPPY_CHILD) ne recrutent pas. Le recruteur
    crée les (TEAM_TARGET-1) coéquipiers et SURVEILLE leurs process : dès qu'un
    enfant meurt (son joueur est mort → le process se termine), il en relance un
    (Fork d'un œuf + nouveau process) pour revenir à 9. Tout passe par des
    commandes de jeu légales (Fork) + la gestion locale de ses sous-process ;
    aucune donnée de jeu n'est échangée hors du serveur.
    """

    def __init__(self, client: Client):
        self.client = client
        # Recruteur = process lancé directement (pas un enfant _spawn).
        self.is_recruiter = os.environ.get("ZAPPY_CHILD") is None
        # Handles des coéquipiers créés par CE recruteur.
        self.children = []
        # Slots initiaux (-c) encore libres au moment de notre connexion : autant
        # de coéquipiers qu'on peut créer par simple connexion (sans Fork).
        self.free_slots = client.free_slots if self.is_recruiter else 0
        self.slots_used = 0
        self.next_build = 0.0

    def tick(self):
        if not self.is_recruiter:
            return
        # On retire les enfants morts (process terminé = joueur mort).
        self.children = [c for c in self.children if c.poll() is None]
        # Équipe complète ? rien à faire.
        if len(self.children) >= TEAM_TARGET - 1:
            return
        if time.time() < self.next_build:
            return
        # Un seul œuf en vol à la fois (régulation par le serveur).
        if self.client.connect_nbr() > 0:
            return
        if self.slots_used < self.free_slots:
            # Il reste un slot initial : l'enfant s'y connectera (pas besoin de Fork).
            self.slots_used += 1
        else:
            # Plus de slot initial → on crée un œuf pour l'enfant.
            self.client.send("Fork")
        self.children.append(_spawn(self.client))
        self.next_build = time.time() + BUILD_FORK_COOLDOWN
