import os
import socket
import time


class Client:
    # The server ALREADY applies a time cost per action (Forward/Look=7 ticks,
    # Inventory=1, Fork=42, Incantation=300), paced by the -f frequency: the AI is
    # therefore naturally paced by the server (each send blocks until the
    # answer, which only arrives once the action is done). ZAPPY_AI_DELAY is an
    # optional EXTRA client-side throttle, only to slow down the visual demo
    # (duration of one move in s; 0.5 -> ~2 tiles/s). 0 (default) = no
    # client throttle, follow the server's pace.
    MOVE_INTERVAL = float(os.getenv("ZAPPY_AI_DELAY", "0") or 0)
    # Only one-tile moves are throttled; rotations stay fast.
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
        self._pending_level = None   # level received via "Current level" (incantation)
        self._next_move = 0.0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))

        welcome = self._read_line()
        if not welcome.startswith("WELCOME"):
            raise ConnectionError(f"Handshake failed: expected WELCOME, got '{welcome}'")

        self._write(self.team)
        # CLIENT-NUM = number of initial slots still free in the team.
        # 0 -> we took the last slot (c=1: we are alone, we must fork).
        # >0 -> more connections are still possible (c=N, test with N instances).
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

    def send_batch(self, commands: list) -> list:
        # Send commands in chunks of 10 (server limit) without waiting for
        # answers in between, then read all answers in order.
        # Goes through _write, which still throttles Forward (ZAPPY_AI_DELAY):
        # the demo stays watchable, and at full speed (delay=0) it is fast.
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
        # return the new level received via "Current level" (or None), then clear it
        lvl = self._pending_level
        self._pending_level = None
        return lvl

    def connect_nbr(self) -> int:
        # Number of the team's eggs (from Fork) still waiting to hatch,
        # as reported BY THE SERVER. This is our only legitimate headcount
        # source (out-of-game communication is not allowed). Acts as a
        # semaphore: if > 0, an egg is already pending, no need to fork another.
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
        # Throttle moves only: a move command is only sent after
        # MOVE_INTERVAL, which caps the visible speed (2 tiles/s at
        # 0.5). Other commands (Look, Inventory, Take...) stay fast.
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
                raise ConnectionError("Connection closed by the server")
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
                # incantation result: may arrive unsolicited (participating player
                # that did not send Incantation), so keep it for the brain
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
                raise ConnectionError("Starved to death or game over")
            else:
                return line