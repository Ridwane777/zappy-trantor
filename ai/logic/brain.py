from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate
from network.broadcast import decode
from logic.fork import TeamManager
from logic.player import ELEVATION_REQ

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
        self.client = client
        self.player = Player()

    def _handle_broadcasts(self):
        # Rassemblement par ancre élue : parmi les NEED de notre niveau, on repère
        # le plus haut weight. Si > notre weight, on se dirige vers son émetteur
        # (il est "au-dessus" de nous → c'est notre ancre). Si on est le plus haut
        # weight, personne n'est au-dessus → on reste immobile (on EST l'ancre).
        # Un seul attracteur global par niveau → convergence sans oscillation.
        best_dir = None
        best_weight = self.player.rally_weight
        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if parsed is None:
                continue
            direction, msg_type, data = parsed

            try:
                sender_team = data.split(":", 1)[0]
            except (AttributeError, IndexError):
                sender_team = ""

            if msg_type == "NEED" and sender_team == self.client.team:
                parts = data.split(":")
                try:
                    sender_weight = int(parts[2])
                except (ValueError, IndexError):
                    continue
                # Ralliement GLOBAL (comme la version de référence, qui montait
                # plus haut) : tout le monde converge vers le joueur du plus haut
                # weight de l'équipe, TOUS NIVEAUX confondus → un seul gros tas où
                # chaque sous-groupe de même niveau se retrouve et incante sur
                # place. Concentrer tout le monde au même endroit donne plus de
                # chances de réunir le quorum que des petits tas par niveau.
                # direction 0 = même tuile : inutile de bouger.
                if direction != 0 and sender_weight > best_weight:
                    best_weight = sender_weight
                    best_dir = direction

            elif msg_type == "HERE" and sender_team == self.client.team and direction == 0:
                self.player.here_count += 1

        # Élection d'ancre : si on n'a entendu aucun NEED de poids supérieur au
        # nôtre (best_weight inchangé == notre rally_weight), c'est qu'on est le
        # plus haut poids du niveau → on est l'ancre. Seule l'ancre déclenche le
        # réapprovisionnement (côté elevate), ce qui sérialise les forks en plus
        # du verrou serveur Connect_nbr.
        self.player.is_anchor = (best_dir is None)

        # On ne rejoint le rdv QUE si on a fini de collecter (stockpile complet) et
        # qu'on n'est pas en ravitaillement (hystérésis food). Sinon le homing
        # tirait les joueurs en pleine collecte loin de leurs pierres.
        ready = (
            not self.player.missing_stockpile()
            and not self.player.need_fuel
            and ELEVATION_REQ.get(self.player.level, {}).get("players", 1) > 1
        )
        # best_dir vaut None (on est l'ancre) ou 0 (l'ancre est sur notre tuile) →
        # pas de déplacement ; 1-8 → on se dirige vers l'ancre globale du niveau.
        if best_dir and ready:
            for move in DIRECTION_MOVES.get(best_dir, []):
                self.client.send(move)

    def run(self):
        # Gestion d'effectif : le recruteur (process lancé à la main) construit
        # l'équipe à TEAM_TARGET=9 et la MAINTIENT (remplace les morts). Plafonné à
        # 9 par équipe. Les enfants (ZAPPY_CHILD) ne recrutent pas, ils jouent.
        team = TeamManager(self.client)

        while True:
            team.tick()
            self._handle_broadcasts()

            lvl = self.client.get_level_update()
            if lvl is not None:
                self.player.level = lvl
                # Nouveau niveau (élevé par l'incantation d'un autre) → on
                # réinitialise le timer de rassemblement.
                self.player.wander_tick = 0

            raw = self.client.send("Inventory")
            self.player.update_inventory(raw)

            # REINE : le recruteur ne joue PAS comme les autres. Son seul rôle est
            # de SURVIVRE et de maintenir l'effectif (team.tick ci-dessus reforke
            # les morts). Elle mange en priorité et n'évolue pas → elle ne meurt
            # quasiment jamais → l'équipe reste peuplée. Les ouvriers (les 8 autres)
            # évoluent ; 8 ≥ 6 suffit pour les quorums des hauts niveaux. Sans ça,
            # si le recruteur mourait, son équipe cessait d'être réapprovisionnée.
            if team.is_recruiter:
                survive(self.client, self.player)
                continue

            if self.player.level >= 8:
                break

            self.player.is_elevating_wait = False

            # (L'effectif est entièrement géré par TeamManager.tick() ci-dessus :
            # construction à 9 + remplacement des morts, plafonné à 9. Plus aucun
            # fork ailleurs.)

            # Décision nourriture/action.
            # Un joueur PRÊT à incanter (plus rien à collecter, niveau exigeant
            # plusieurs joueurs) doit RESTER pour servir de point de ralliement
            # stable — y compris l'ancre, qui sinon repartirait manger en boucle
            # (solo) et le rendez-vous ne se formerait jamais. Il ne mange donc
            # qu'en danger critique. Les autres (en collecte, ou helpers niveau 1
            # qui incantent en solo) gardent un buffer épais (seuil normal).
            missing = self.player.missing_stockpile()
            req = ELEVATION_REQ.get(self.player.level, {})
            multi = req.get("players", 1) > 1
            food = self.player.inventory["food"]

            # Hystérésis de ravitaillement : plein sous CRITICAL, stop à
            # FOOD_RALLY_TARGET ; entre les deux on conserve l'état précédent.
            if food < self.player.CRITICAL_FOOD:
                self.player.need_fuel = True
            elif food >= self.player.FOOD_RALLY_TARGET:
                self.player.need_fuel = False

            if (not missing) and multi:
                # Pierres réunies : hystérésis food (plein à 50, ne repart qu'à 12)
                # → on reste au rdv tant qu'on n'est pas retombé sous le seuil bas.
                must_eat = self.player.need_fuel
            else:
                # En collecte (ou niveau solo) : buffer normal.
                must_eat = self.player.needs_food()

            if must_eat:
                survive(self.client, self.player)
            elif missing:
                collect(self.client, self.player)
            else:
                elevate(self.client, self.player)
