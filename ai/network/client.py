import socket


class Client:
    def __init__(self, host: str, port: int, team: str):
        self.host = host
        self.port = port
        self.team = team
        self.sock = None

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
