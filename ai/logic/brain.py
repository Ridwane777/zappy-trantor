from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate
from network.broadcast import decode, encode
from logic.fork import TeamManager
from logic.player import ELEVATION_REQ

# Traduction d'une direction de son (entendue via Broadcast) en mouvements pour se
# rapprocher de l'émetteur.
#
# Le serveur indique d'OÙ vient un son par un numéro de case K relatif à
# l'orientation de celui qui écoute :
#       K = 1 -> devant            K = 5 -> derrière
#       K = 3 -> à droite          K = 7 -> à gauche
#       (2,4,6,8 = diagonales, qu'on approxime en tournant vers le cardinal le
#        plus proche puis en avançant)
#       K = 0 -> même case que l'émetteur (rien à faire, on est arrivé)
#
# Pour chaque K on donne donc la petite séquence de commandes qui oriente le
# joueur dans la bonne direction puis le fait avancer d'une case.
DIRECTION_MOVES = {
    0: [],                                  # déjà sur la case de l'émetteur
    1: ["Forward"],                         # devant
    2: ["Right", "Forward"],                # devant-droite
    3: ["Right", "Forward"],                # droite
    4: ["Right", "Right", "Forward"],       # derrière-droite
    5: ["Right", "Right", "Forward"],       # derrière
    6: ["Left", "Left", "Forward"],         # derrière-gauche
    7: ["Left", "Forward"],                 # gauche
    8: ["Left", "Forward"],                 # devant-gauche
}

# Réserve de food que la REINE maintient en permanence. Sous ce seuil elle part
# se nourrir ; au-dessus elle reste immobile (elle ne gaspille pas et ne prive pas
# les ouvriers de food). Élevé exprès : la survie de la reine est la condition de
# tout le système, car c'est elle qui réapprovisionne l'équipe.
QUEEN_FOOD_MIN = 30


class Brain:
    """Boucle de décision d'un joueur : à chaque tour, observer puis agir.

    Le déroulé d'un tour est toujours le même :
      1. la reine (s'il s'agit du process recruteur) maintient l'effectif ;
      2. on écoute les sons (broadcasts) pour décider du regroupement ;
      3. on relit niveau + inventaire depuis le serveur ;
      4. on choisit UNE action : manger, collecter, se regrouper, ou incanter.
    """

    def __init__(self, client: Client):
        self.client = client
        self.player = Player()

    def _handle_broadcasts(self):
        """Écoute les sons de l'équipe et décide du regroupement (ancre/suiveur).

        Principe du regroupement par ANCRE ÉLUE :
          - chaque joueur diffuse en continu "NEED:equipe:niveau:poids" ;
          - parmi tous les NEED reçus DE MON NIVEAU (toutes équipes confondues, car
            une incantation réunit les joueurs de même niveau quelle que soit leur
            équipe), on repère le plus grand poids ;
          - si ce plus grand poids est le mien → personne au-dessus de moi → JE suis
            l'ancre : je reste immobile et je sers de point de rendez-vous ;
          - sinon → je me dirige vers l'émetteur du plus grand poids (mon ancre).
        Comme le poids est un tirage aléatoire FIXE, l'ancre d'un niveau est unique
        et stable : tout le monde converge vers le même joueur, sans osciller.
        """
        # best_weight = le plus grand poids entendu jusqu'ici (on part du nôtre).
        # best_dir = direction du joueur qui porte ce plus grand poids :
        #   None -> personne de plus fort entendu (c'est donc NOUS l'ancre)
        #   0    -> ce joueur est sur NOTRE case (on est déjà au rendez-vous)
        #   1..8 -> direction vers laquelle se déplacer pour le rejoindre
        best_dir = None
        best_weight = self.player.rally_weight

        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if parsed is None:
                continue
            direction, msg_type, data = parsed
            if msg_type != "NEED":
                continue

            # data = "equipe:niveau:poids"
            parts = data.split(":")
            try:
                sender_level = int(parts[1])
                sender_weight = int(parts[2])
            except (ValueError, IndexError):
                continue

            # On n'écoute que les joueurs de NOTRE niveau : ce sont les seuls avec
            # qui on peut incanter.
            if sender_level != self.player.level:
                continue

            # On retient le plus haut poids, MÊME s'il vient de notre propre case
            # (direction 0). C'est essentiel : un suiveur déjà arrivé au rendez-vous
            # entend l'ancre en direction 0 et doit RESTER là. Sans ça, il
            # repartirait vers le 2e plus haut poids resté au loin (bug du joueur
            # "réaspiré" qui empêchait les groupes de se former).
            if sender_weight > best_weight:
                best_weight = sender_weight
                best_dir = direction

        # Si aucun poids supérieur au nôtre n'a été entendu, c'est nous l'ancre.
        self.player.is_anchor = (best_dir is None)

        # On ne se déplace vers l'ancre QUE si on est prêt : plus rien à collecter
        # pour le niveau courant ET pas en train de refaire le plein de food. Sinon
        # le ralliement arracherait un joueur en pleine collecte loin de ses
        # pierres. (best_dir vaut 0 quand on est déjà sur la case de l'ancre : 0 est
        # "falsy", donc on ne bouge pas — on reste au rendez-vous.)
        ready = (
            not self.player.missing_for_elevation()
            and not self.player.need_fuel
            and ELEVATION_REQ.get(self.player.level, {}).get("players", 1) > 1
        )
        if best_dir and ready:
            for move in DIRECTION_MOVES.get(best_dir, []):
                self.client.send(move)

    def run(self):
        """Boucle principale : un tour de jeu par itération, jusqu'au niveau 8."""
        # Gestion de l'effectif. Le process lancé à la main (sans la variable
        # d'environnement ZAPPY_CHILD) est la REINE de l'équipe ; les process qu'il
        # crée sont les ouvriers. Voir fork.py pour le détail.
        team = TeamManager(self.client)

        while True:
            team.tick()

            # === CAS DE LA REINE ===
            # La reine a un seul rôle : survivre et maintenir l'effectif (fait par
            # team.tick() juste au-dessus). Elle ne collecte pas, ne se regroupe pas
            # et N'ÉVOLUE PAS → elle ne prend aucun risque → elle ne meurt
            # quasiment jamais → l'équipe est réapprovisionnée en permanence.
            # Comme les incantations sont inter-équipes (on monte avec les joueurs
            # de même niveau de TOUTES les équipes), sacrifier 1 évolueur par équipe
            # ne coûte rien : le quorum se forme sur le total de toutes les équipes.
            if team.is_recruiter:
                raw = self.client.send("Inventory")
                self.player.update_inventory(raw)
                if self.player.inventory["food"] < QUEEN_FOOD_MIN:
                    survive(self.client, self.player)
                continue

            # === CAS D'UN OUVRIER ===
            # 1) Écouter les sons et, le cas échéant, avancer vers l'ancre.
            self._handle_broadcasts()

            # 2) Le serveur a-t-il monté notre niveau ? (cela arrive quand un AUTRE
            #    joueur lance l'incantation à laquelle on participe.)
            lvl = self.client.get_level_update()
            if lvl is not None:
                self.player.level = lvl

            # 3) Relire l'inventaire (source de vérité pour food et pierres).
            raw = self.client.send("Inventory")
            self.player.update_inventory(raw)

            # 4) Objectif atteint : on a fini.
            if self.player.level >= 8:
                break

            # --- Préparer la décision ---
            missing = self.player.missing_for_elevation()
            req = ELEVATION_REQ.get(self.player.level, {})
            multi = req.get("players", 1) > 1          # ce niveau exige-t-il un groupe ?
            food = self.player.inventory["food"]

            # Beacon de présence : à un niveau de groupe, on diffuse niveau + poids
            # à CHAQUE tour. C'est ce qui rend l'élection d'ancre GLOBALE et STABLE
            # (l'ancre = le plus haut poids du niveau, qui ne change pas selon qui
            # vient de finir sa collecte) et donne aux suiveurs une direction
            # fraîche à chaque pas pour converger.
            if multi:
                self.client.send(
                    f"Broadcast {encode('NEED', f'{self.client.team}:{self.player.level}:{self.player.rally_weight}')}"
                )

            # Mise à jour de l'hystérésis de ravitaillement (voir Player.need_fuel) :
            # on bascule en "refaire le plein" sous le seuil critique, et on n'en
            # sort qu'une fois la réserve cible atteinte.
            if food < self.player.CRITICAL_FOOD:
                self.player.need_fuel = True
            elif food >= self.player.FOOD_RALLY_TARGET:
                self.player.need_fuel = False

            # --- Choisir UNE action pour ce tour ---
            if multi and self.player.is_anchor:
                # ANCRE d'un niveau de groupe : c'est le point de rendez-vous figé.
                # Elle ne collecte ni n'erre jamais ; elle reste sur place pour que
                # les suiveurs convergent vers une cible immobile, et elle incante
                # dès que le quorum est réuni. Elle ne se déplace QUE sous famine
                # critique (puis reviendra se figer une fois rassasiée).
                if food < self.player.CRITICAL_FOOD:
                    survive(self.client, self.player)
                else:
                    elevate(self.client, self.player)
            else:
                # Suiveur, ou joueur d'un niveau "solo" (niveau 1).
                if (not missing) and multi:
                    # Pierres déjà réunies : on est en attente au rendez-vous. On ne
                    # repart manger que si la réserve est vraiment basse (hystérésis)
                    # pour ne pas casser le regroupement.
                    must_eat = self.player.need_fuel
                else:
                    # En collecte (ou niveau solo) : on garde le coussin normal.
                    must_eat = self.player.needs_food()

                if must_eat:
                    survive(self.client, self.player)
                elif missing:
                    collect(self.client, self.player)
                else:
                    elevate(self.client, self.player)
