import socket


class Client:
    MAX_QUEUE = 10

    def __init__(self, host: str, port: int, team: str):
        self.host = host
        self.port = port
        self.team = team
        self.sock = None
        self.buffer = ""
        self.pending = 0
        self.world_x = 0
        self.world_y = 0
        self.slots = 0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self._wait_line()          # WELCOME
        self._send(self.team)
        self.slots = int(self._wait_line())
        dims = self._wait_line().split()
        self.world_x, self.world_y = int(dims[0]), int(dims[1])

    def send(self, command: str) -> str:
        if self.pending >= self.MAX_QUEUE:
            raise RuntimeError("command queue full")
        self._send(command)
        self.pending += 1
        response = self._wait_line()
        self.pending -= 1
        if response == "dead":
            raise SystemExit("dead")
        return response

    def _send(self, msg: str):
        self.sock.sendall((msg + "\n").encode())

    def _wait_line(self) -> str:
        while "\n" not in self.buffer:
            chunk = self.sock.recv(4096).decode()
            if not chunk:
                raise SystemExit("server disconnected")
            self.buffer += chunk
        line, self.buffer = self.buffer.split("\n", 1)
        return line.strip()
