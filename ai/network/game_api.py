"""Game command API: the only place that writes protocol commands.

Strategy code calls `api.take("food")`, never `client.request("Take food")`, and
gets parsed results (tiles, inventory, new level) instead of raw text.
"""
from network.client import Client
from network.protocol import parse_inventory, parse_level, parse_look


class GameApi:
    def __init__(self, client: Client):
        self._client = client

    @property
    def team(self) -> str:
        return self._client.team

    @property
    def free_slots(self) -> int:
        return self._client.free_slots

    # ----- moves --------------------------------------------------------------------

    def walk(self, moves: list):
        """Runs a list of "Forward" / "Left" / "Right" moves, pipelined."""
        unknown = [m for m in moves if m not in ("Forward", "Left", "Right")]
        if unknown:
            raise ValueError(f"not a move: {unknown}")
        if moves:
            self._client.request_many(moves)

    # ----- observation --------------------------------------------------------------

    def look(self) -> list:
        return parse_look(self._client.request("Look"))

    def inventory(self) -> dict:
        return parse_inventory(self._client.request("Inventory"))

    def connect_nbr(self) -> int:
        reply = self._client.request("Connect_nbr")
        return int(reply) if reply.isdigit() else 0

    # ----- objects ------------------------------------------------------------------

    def take(self, item: str) -> bool:
        return self._client.request(f"Take {item}") == "ok"

    def drop(self, item: str) -> bool:
        return self._client.request(f"Set {item}") == "ok"

    # ----- team ---------------------------------------------------------------------

    def broadcast(self, text: str):
        if "\n" in text:
            raise ValueError("a broadcast cannot contain a newline")
        self._client.request(f"Broadcast {text}")

    def fork(self):
        self._client.request("Fork")

    def messages(self) -> list:
        """Broadcasts heard since the last call: [(direction, text), ...]."""
        return self._client.take_messages()

    def announced_level(self):
        """Level reached through someone else's incantation, if any (else None)."""
        return self._client.take_level()

    def incant(self) -> int:
        """Starts an incantation and waits for its end. Returns the new level, or 0."""
        start = self._client.request("Incantation", accept=("Elevation underway", "Current level:"))
        if start == "ko":
            return 0
        end = start if start.startswith("Current level:") else self._client.reply(accept=("Current level:",))
        return parse_level(end) or 0
