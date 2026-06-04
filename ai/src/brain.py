from src.client import Client
from src.player import Player
from src.states.survive import survive
from src.states.collect import collect
from src.states.elevate import elevate


class Brain:
    def __init__(self, client: Client):
        self.client = client
        self.player = Player()

    def run(self):
        while True:
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
