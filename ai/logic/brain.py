import time

from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate
from network.broadcast import decode
from logic.fork import do_fork, do_initial_fork, FORK_COOLDOWN_SEC
from logic.player import ELEVATION_REQ

DIRECTION_MOVES = {
    0: [],
    1: ["Forward"],
    2: ["Right", "Forward"],
    3: ["Right", "Forward"],
    4: ["Right", "Right", "Forward"],
    5: ["Right", "Right", "Forward"],
    6: ["Left", "Left", "Forward"],
    7: ["Left", "Forward"],
    8: ["Left", "Forward"],
}

# Délai maximum d'attente (mode WAIT) avant de reprendre quoi qu'il arrive.
_WAIT_TIMEOUT = 300.0


class Brain:
    def __init__(self, client: Client, initial_team_size: int = 1):
        self.client = client
        self.player = Player(initial_team_size=initial_team_size)

    def _handle_broadcasts(self):
        # Rassemblement par ancre élue : parmi les NEED de notre niveau, on repère
        # le plus haut weight. Si > notre weight, on se dirige vers son émetteur
        # (il est "au-dessus" de nous → c'est notre ancre). Si on est le plus haut
        # weight, personne n'est au-dessus → on reste immobile (on EST l'ancre).
        # Un seul attracteur global par niveau → convergence sans oscillation.
        best_dir = None
        best_weight = self.player.rally_weight
        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if parsed is None:
                continue
            direction, msg_type, data = parsed

            try:
                sender_team = data.split(":", 1)[0]
            except (AttributeError, IndexError):
                sender_team = ""

            if msg_type == "NEED" and sender_team == self.client.team:
                if self.player.is_waiting:
                    continue
                parts = data.split(":")
                try:
                    sender_level = int(parts[1])
                    sender_weight = int(parts[2])
                except (ValueError, IndexError):
                    continue
                if sender_level != self.player.level:
                    continue
                # direction 0 = même tuile : inutile de bouger.
                if direction != 0 and sender_weight > best_weight:
                    best_weight = sender_weight
                    best_dir = direction

            elif msg_type == "STUCK" and sender_team == self.client.team:
                # Un coéquipier est bloqué seul à un niveau inférieur au nôtre.
                # On passe en mode attente passive (survive only) jusqu'à ce qu'il
                # nous rejoigne (RESUME reçu) ou que le timeout expire.
                try:
                    stuck_level = int(data.split(":", 1)[1])
                except (ValueError, IndexError):
                    stuck_level = 0
                if self.player.level > stuck_level and not self.player.is_waiting:
                    self.player.is_waiting = True
                    self.player.wait_for_level = self.player.level
                    self.player.wait_until = time.time() + _WAIT_TIMEOUT

            elif msg_type == "RESUME" and sender_team == self.client.team:
                # Le joueur bloqué vient d'avancer : si son nouveau niveau atteint
                # le nôtre, on reprend le jeu normal.
                try:
                    resume_level = int(data.split(":", 1)[1])
                except (ValueError, IndexError):
                    resume_level = 0
                if self.player.is_waiting and resume_level >= self.player.wait_for_level:
                    self.player.is_waiting = False

            elif msg_type == "HERE" and sender_team == self.client.team and direction == 0:
                self.player.here_count += 1

            elif msg_type == "FORK" and sender_team == self.client.team:
                try:
                    _, size_part = data.split(":", 1)
                    self.player.known_team_size = max(
                        self.player.known_team_size, int(size_part)
                    )
                except (ValueError, TypeError):
                    pass
                self.player.fork_until = max(
                    self.player.fork_until, time.time() + FORK_COOLDOWN_SEC
                )

        if best_dir is not None:
            for move in DIRECTION_MOVES.get(best_dir, []):
                self.client.send(move)

    def run(self):
        while True:
            self._handle_broadcasts()

            lvl = self.client.get_level_update()
            if lvl is not None:
                self.player.level = lvl

            raw = self.client.send("Inventory")
            self.player.update_inventory(raw)

            if self.player.level >= 8:
                break

            self.player.is_elevating_wait = False

            # Fork initial : uniquement quand free_slots==0 (le serveur n'a plus
            # de slot libre — cas c=1 où on est seul) ET qu'on sait qu'on n'est
            # pas un enfant forké (initial_team_size==1).
            # Si free_slots>0 (test avec N instances manuelles), on ne forke pas :
            # les autres instances connectent directement, pas via fork.
            if (self.player.known_team_size < 6
                    and self.player.initial_team_size == 1
                    and self.client.free_slots == 0
                    and time.time() >= self.player.fork_until):
                do_initial_fork(self.client, self.player)
                continue

            # Mode WAIT : un coéquipier est bloqué seul, on l'attend.
            if self.player.is_waiting:
                if time.time() >= self.player.wait_until:
                    self.player.is_waiting = False
                else:
                    if self.player.needs_food():
                        survive(self.client, self.player)
                    continue

            # Décision nourriture/action.
            # Un joueur PRÊT à incanter (plus rien à collecter, niveau exigeant
            # plusieurs joueurs) doit RESTER pour servir de point de ralliement
            # stable — y compris l'ancre, qui sinon repartirait manger en boucle
            # (solo) et le rendez-vous ne se formerait jamais. Il ne mange donc
            # qu'en danger critique. Les autres (en collecte, ou helpers niveau 1
            # qui incantent en solo) gardent un buffer épais (seuil normal).
            missing = self.player.missing_for_elevation()
            req = ELEVATION_REQ.get(self.player.level, {})
            ready_to_rally = (not missing) and req.get("players", 1) > 1
            critical = self.player.inventory["food"] < self.player.CRITICAL_FOOD

            if ready_to_rally:
                must_eat = critical
            else:
                must_eat = self.player.needs_food()

            if must_eat:
                survive(self.client, self.player)
            elif missing:
                collect(self.client, self.player)
            else:
                elevate(self.client, self.player)
