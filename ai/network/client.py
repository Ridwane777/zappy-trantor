import socket


class Client:
    def __init__(self, host: str, port: int, team: str):
        self.host = host
        self.port = port
        self.team = team
        self.sock = None