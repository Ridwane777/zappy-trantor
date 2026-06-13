import socket


class Client:
    def __init__(self, host: str, port: int, team: str):
        self.host = host
        self.port = port
        self.team = team
        self.sock = None
        self.map_x = 0
        self.map_y = 0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))

        welcome = self._read_line()
        if not welcome.startswith("WELCOME"):
            raise ConnectionError(f"Handshake raté : attendu WELCOME, reçu '{welcome}'")

        self._write(self.team)
        self._read_line()  # CLIENT-NUM

        dims = self._read_line().split()
        self.map_x = int(dims[0])
        self.map_y = int(dims[1])

    def _write(self, message: str):
        self.sock.sendall((message + "\n").encode())

    def _read_line(self) -> str:
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = self.sock.recv(1)
            if not chunk:
                raise ConnectionError("Connexion fermée par le serveur")
            buf += chunk
        return buf.decode().strip()