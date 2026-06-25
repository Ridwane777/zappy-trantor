from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate
from logic.fork import should_fork, do_fork
from network.broadcast import decode, encode

# mouvements à effectuer selon la direction K du broadcast (schéma du sujet)
# K=0 : même tuile, K=1 : devant, K=2 : avant-droite, ... K=8 : avant-gauche
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
    def __init__(self, client: Client):
        # le client TCP pour envoyer des commandes au serveur
        self.client = client
        # l'état du joueur que le brain va consulter à chaque tour
        self.player = Player()

    def _handle_broadcasts(self):
        # le client accumule les broadcasts reçus entre les réponses. On ne
        # réagit qu'au DERNIER appel pertinent du tour : répondre à chaque
        # message de la file ferait pivoter le joueur sur place sans avancer.
        target = None
        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if parsed is None:
                continue
            direction, msg_type, data = parsed
            # si un coéquipier du même niveau appelle au rassemblement
            if msg_type == "NEED" and data == str(self.player.level):
                target = direction
            # un coéquipier signale son arrivée sur notre tuile (direction=0)
            elif msg_type == "HERE" and data == str(self.player.level) and direction == 0:
                self.player.here_count += 1
        if target is not None:
            for move in DIRECTION_MOVES.get(target, []):
                self.client.send(move)
            # on prévient le leader qu'on est arrivé (compté seulement s'il nous voit
            # sur sa tuile, direction=0 de son point de vue)
            self.client.send(f"Broadcast {encode('HERE', str(self.player.level))}")

    def run(self):
        while True:
            # on traite les broadcasts reçus depuis le dernier tour
            self._handle_broadcasts()

            # élévation réussie (initiateur OU participant) : on applique le niveau
            # que le serveur nous a envoyé via "Current level"
            lvl = self.client.get_level_update()
            if lvl is not None:
                self.player.level = lvl

            # on demande l'inventaire au serveur pour avoir l'état à jour
            raw = self.client.send("Inventory")
            self.player.update_inventory(raw)

            # condition de victoire : 6 joueurs niveau 8, on arrête
            if self.player.level >= 8:
                break

            # gestion du fork : décrémente le cooldown puis pond un œuf + lance un
            # nouvel AI si on a de quoi (atteindre le nb de joueurs requis aux hauts
            # niveaux d'incantation)
            if self.player.fork_cooldown > 0:
                self.player.fork_cooldown -= 1
            if should_fork(self.player):
                do_fork(self.client, self.player)

            # priorité 1 : survie — si peu de food, on va manger
            if self.player.needs_food():
                survive(self.client, self.player)

            # priorité 2 : collecte — si des pierres manquent, on va les chercher
            elif self.player.missing_for_elevation():
                collect(self.client, self.player)

            # priorité 3 : élévation — tout est réuni, on tente l'incantation
            else:
                elevate(self.client, self.player)
