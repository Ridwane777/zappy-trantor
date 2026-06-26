import time

from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate
from network.broadcast import decode, encode

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


class Brain:
    def __init__(self, client: Client, initial_team_size: int = 1):
        self.client = client
        self.player = Player(initial_team_size=initial_team_size)

    def _handle_broadcasts(self):
        target = None
        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if parsed is None:
                continue
            direction, msg_type, data = parsed

            try:
                sender_team = data.split(":", 1)[0]
            except (AttributeError, IndexError):
                sender_team = ""

            # n'importe quel coéquipier (tous niveaux) peut aider :
            # on filtre par team uniquement, pas par niveau
            if msg_type == "NEED" and sender_team == self.client.team:
                target = direction
            elif msg_type == "HERE" and sender_team == self.client.team and direction == 0:
                self.player.here_count += 1
            # broadcast fork d'un coéquipier : on met à jour notre estimation
            # de la taille de la team et on bloque notre propre fork un moment
            elif msg_type == "FORK":
                from logic.fork import FORK_COOLDOWN_SEC
                try:
                    team_part, size_part = data.split(":", 1)
                    if team_part == self.client.team:
                        self.player.known_team_size = max(
                            self.player.known_team_size, int(size_part)
                        )
                except (ValueError, TypeError):
                    pass
                self.player.fork_until = max(
                    self.player.fork_until, time.time() + FORK_COOLDOWN_SEC
                )

        if target is not None:
            for move in DIRECTION_MOVES.get(target, []):
                self.client.send(move)
            self.client.send(
                f"Broadcast {encode('HERE', f'{self.client.team}:{self.player.level}')}"
            )

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

            if self.player.needs_food():
                survive(self.client, self.player)
            elif self.player.missing_for_elevation():
                collect(self.client, self.player)
            else:
                elevate(self.client, self.player)
