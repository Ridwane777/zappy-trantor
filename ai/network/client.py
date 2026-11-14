"""Transport layer: one TCP connection to the server, line by line.

It sends commands, returns their replies, and sets aside the asynchronous lines
the server can push at any time (broadcasts, level-ups, ejections, death). It knows
the wire format of lines, not the meaning of game commands: those are in game_api.py.
"""
import os
import socket
import time

from network.protocol import parse_level, parse_message

# Prefixes of lines that are not the reply to the pending command, unless the
# caller explicitly waits for one of them (an incantation waits for both).
_ASYNC_PREFIXES = ("message ", "Current level:", "Elevation underway", "eject:")


class ServerClosed(ConnectionError):
    """The server closed the connection, or the player died."""


class Client:
    def __init__(self, host: str, port: int, team: str, move_delay: float = None, debug: bool = None):
        self.host = host
        self.port = port
        self.team = team
        # Optional client-side pacing of moves, to make demos watchable (seconds per Forward).
        self._move_delay = float(os.getenv("ZAPPY_AI_DELAY", "0") or 0) if move_delay is None else move_delay
        self._debug = os.getenv("ZAPPY_DEBUG") == "1" if debug is None else debug
        self._sock = None
        self._buffer = b""
        self._next_move = 0.0
        self._messages = []        # (direction, text) broadcasts not yet consumed
        self._level = None         # last level announced asynchronously
        self.free_slots = 0
        self.map_size = (0, 0)

    def connect(self):
        self._sock = socket.create_connection((self.host, self.port))
        if self._read_line() != "WELCOME":
            raise ServerClosed("handshake failed: no WELCOME")
        self._write(self.team)
        first = self._read_line()
        if first == "ko":
            raise ServerClosed(f"team {self.team!r} is full or unknown")
        self.free_slots = int(first) if first.isdigit() else 0
        width, height = self._read_line().split()
        self.map_size = (int(width), int(height))

    # ----- commands ---------------------------------------------------------------

    def request(self, command: str, accept=()) -> str:
        """Sends one command and returns its reply. `accept` lists asynchronous
        prefixes that count as a reply for this command (used by Incantation)."""
        self._write(command)
        return self.reply(accept)

    def request_many(self, commands: list) -> list:
        """Pipelines commands, at most 10 in flight (the server's queue size)."""
        replies = []
        for i in range(0, len(commands), 10):
            chunk = commands[i:i + 10]
            for command in chunk:
                self._write(command)
            replies.extend(self.reply() for _ in chunk)
        return replies

    def reply(self, accept=()) -> str:
        """Next line that answers the pending command."""
        while True:
            line = self._read_line()
            if line == "dead":
                raise ServerClosed("the player died")
            if line.startswith(tuple(accept)):
                return line
            if line.startswith(_ASYNC_PREFIXES):
                self._keep_async(line)
                continue
            return line

    # ----- asynchronous events ----------------------------------------------------

    def take_messages(self) -> list:
        """Broadcasts received since the last call, oldest first."""
        messages, self._messages = self._messages, []
        return messages

    def take_level(self):
        """Level announced by an incantation we took part in, or None."""
        level, self._level = self._level, None
        return level

    def _keep_async(self, line: str):
        message = parse_message(line)
        if message:
            self._messages.append(message)
            return
        level = parse_level(line)
        if level is not None:
            self._level = level

    # ----- wire -------------------------------------------------------------------

    def _write(self, line: str):
        if self._move_delay > 0 and line == "Forward":
            wait = self._next_move - time.monotonic()
            if wait > 0:
                time.sleep(wait)
            self._next_move = time.monotonic() + self._move_delay
        if self._debug:
            print(f">> {line}", flush=True)
        self._sock.sendall(line.encode() + b"\n")

    def _read_line(self) -> str:
        while b"\n" not in self._buffer:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise ServerClosed("connection closed by the server")
            self._buffer += chunk
        line, self._buffer = self._buffer.split(b"\n", 1)
        text = line.decode(errors="replace").strip()
        if self._debug:
            print(f"<< {text}", flush=True)
        return text
