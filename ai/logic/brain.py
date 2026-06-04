from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate


class Brain:
    def __init__(self, client: Client):
        # le client TCP pour envoyer des commandes au serveur
        self.client = client
        # l'état du joueur que le brain va consulter à chaque tour
        self.player = Player()

    def run(self):
        while True:
            # on demande l'inventaire au serveur pour avoir l'état à jour
            raw = self.client.send("Inventory")
            self.player.update_inventory(raw)

            # condition de victoire : 6 joueurs niveau 8, on arrête
            if self.player.level >= 8:
                break

            # priorité 1 : survie — si peu de food, on va manger
            if self.player.needs_food():
                survive(self.client, self.player)

            # priorité 2 : collecte — si des pierres manquent, on va les chercher
            elif self.player.missing_for_elevation():
                collect(self.client, self.player)

            # priorité 3 : élévation — tout est réuni, on tente l'incantation
            else:
                elevate(self.client, self.player)
