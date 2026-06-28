import os
import socket
import time


class Client:
    # Notre serveur n'applique pas de coût en temps par action (la vitesse spec
    # vient de -f) : l'IA tournerait donc à plein régime et les joueurs
    # fileraient jusqu'aux bords de la carte (torus) en se téléportant.
    # ZAPPY_AI_DELAY = durée d'UN déplacement (Forward/Left/Right) en secondes,
    # ce qui cadence la vitesse visible. 0.5 -> ~2 tuiles/seconde. 0 = plein régime.
    MOVE_INTERVAL = float(os.getenv("ZAPPY_AI_DELAY", "0") or 0)
    # On ne cadence que l'avancée d'une tuile : à 0.5 -> 2 tuiles/s pile. Les
    # rotations restent rapides (peu fréquentes), elles n'entament pas le rythme.
    _MOVE_CMDS = ("Forward",)

    def __init__(self, host: str, port: int, team: str):
        self.host = host
        self.port = port
        self.team = team
        self.sock = None
        self.map_x = 0
        self.map_y = 0
        self.free_slots = 0
        self._broadcasts = []
        self._pending_level = None   # niveau reçu via "Current level" (incantation)
        self._next_move = 0.0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))

        welcome = self._read_line()
        if not welcome.startswith("WELCOME"):
            raise ConnectionError(f"Handshake raté : attendu WELCOME, reçu '{welcome}'")

        self._write(self.team)
        # CLIENT-NUM = nombre de slots initiaux encore libres dans la team.
        # 0 → on a pris le dernier slot (cas c=1 : on est seul, il faut forker).
        # >0 → d'autres connexions sont encore possibles (cas c=N, test à N instances).
        try:
            self.free_slots = int(self._read_line())
        except ValueError:
            self.free_slots = 0

        dims = self._read_line().split()
        self.map_x = int(dims[0])
        self.map_y = int(dims[1])

    def send(self, command: str, is_incantation: bool = False) -> str:
        self._write(command)
        return self._read_response(is_incantation)

    def receive(self, is_incantation: bool = False) -> str:
        return self._read_response(is_incantation)

    def send_incantation(self) -> str:
        """Two-phase send for Incantation.

        Phase 1 reads until 'Elevation underway' (start OK) or 'ko' (immediate fail).
        Phase 2 reads until 'Current level: N' (success) or 'ko' (fail after 300 ticks).
        Keeps routing broadcasts/eject/dead normally so participants are not affected.
        """
        self._write("Incantation")

        # Phase 1 — expect "Elevation underway" or "ko"
        # "Current level" peut arriver ici si on était participant d'une
        # incantation concurrente qui vient de se terminer : on le stocke
        # et on continue à lire (notre propre Incantation n'est pas encore traitée)
        while True:
            line = self._read_line()
            if line.startswith("message "):
                self._broadcasts.append(line)
            elif line.startswith("Current level"):
                try:
                    self._pending_level = int(line.split(":")[1].strip())
                except (IndexError, ValueError):
                    pass
            elif line.startswith("eject:"):
                pass
            elif line == "dead":
                raise ConnectionError("Mort de faim ou fin de partie")
            elif line == "ko":
                return "ko"
            else:
                break  # "Elevation underway"

        # Phase 2 — wait for final result (300 ticks later)
        while True:
            line = self._read_line()
            if line.startswith("message "):
                self._broadcasts.append(line)
            elif line.startswith("eject:"):
                pass
            elif line == "dead":
                raise ConnectionError("Mort de faim ou fin de partie")
            elif line == "ko":
                return "ko"
            elif line.startswith("Current level"):
                try:
                    self._pending_level = int(line.split(":")[1].strip())
                except (IndexError, ValueError):
                    pass
                return line

    def send_batch(self, commands: list) -> list:
        # envoie les commandes par chunks de 10 (limite du serveur) sans attendre
        # les réponses entre chaque, puis lit toutes les réponses dans l'ordre.
        # On passe par _write, qui cadence quand même les Forward (ZAPPY_AI_DELAY) :
        # la démo reste regardable, et à plein régime (delay=0) c'est rapide.
        results = []
        for i in range(0, len(commands), 10):
            chunk = commands[i:i + 10]
            for cmd in chunk:
                self._write(cmd)
            for _ in chunk:
                results.append(self._read_response())
        return results

    def get_broadcasts(self) -> list:
        pending = self._broadcasts[:]
        self._broadcasts = []
        return pending

    def get_level_update(self):
        # renvoie le nouveau niveau reçu via "Current level" (ou None), puis l'efface
        lvl = self._pending_level
        self._pending_level = None
        return lvl

    def connect_nbr(self) -> int:
        # Nombre d'œufs (issus de Fork) de l'équipe encore en attente d'éclosion,
        # tel que rapporté PAR LE SERVEUR. C'est notre seule source d'effectif
        # légitime (le sujet interdit toute communication hors-jeu). Sert de
        # sémaphore : si > 0, un œuf est déjà en vol → inutile d'en reforker un.
        raw = self.send("Connect_nbr")
        try:
            return int(raw)
        except (ValueError, TypeError):
            return 0

    _DEBUG = os.getenv("ZAPPY_DEBUG") == "1"

    def _log(self, direction: str, text: str):
        if self._DEBUG:
            print(f"[{direction}] {text}", flush=True)

    def _write(self, message: str):
        # cadençage des déplacements uniquement : un Forward/Left/Right ne part
        # qu'après MOVE_INTERVAL, ce qui borne la vitesse visible (2 tuiles/s à
        # 0.5). Les autres commandes (Look, Inventory, Take...) restent rapides.
        if self.MOVE_INTERVAL > 0 and message.split(" ", 1)[0] in self._MOVE_CMDS:
            wait = self._next_move - time.monotonic()
            if wait > 0:
                time.sleep(wait)
            self._next_move = time.monotonic() + self.MOVE_INTERVAL
        self._log(">>", message)
        self.sock.sendall((message + "\n").encode())

    def _read_line(self) -> str:
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = self.sock.recv(1)
            if not chunk:
                raise ConnectionError("Connexion fermée par le serveur")
            buf += chunk
        line = buf.decode().strip()
        self._log("<<", line)
        return line

    def _read_response(self, is_incantation: bool = False) -> str:
        while True:
            line = self._read_line()
            if line.startswith("message "):
                self._broadcasts.append(line)
            elif line.startswith("Current level"):
                if is_incantation:
                    return line
                # incantation result — peut arriver non sollicité (joueur participant
                # qui n'a pas envoyé Incantation) : on le met de côté pour le brain
                try:
                    self._pending_level = int(line.split(":")[1].strip())
                except (IndexError, ValueError):
                    pass
            elif line.startswith("Elevation underway"):
                if is_incantation:
                    return line
                pass  # Async message from incantation, ignore as command response
            elif line.startswith("eject:"):
                pass  # Async message from being pushed, ignore as command response
            elif line == "dead":
                raise ConnectionError("Mort de faim ou fin de partie")
            else:
                return line