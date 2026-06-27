import random

RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

ELEVATION_REQ = {
    1: {"players": 1, "linemate": 1, "deraumere": 0, "sibur": 0, "mendiane": 0, "phiras": 0, "thystame": 0},
    2: {"players": 2, "linemate": 1, "deraumere": 1, "sibur": 1, "mendiane": 0, "phiras": 0, "thystame": 0},
    3: {"players": 2, "linemate": 2, "deraumere": 0, "sibur": 1, "mendiane": 0, "phiras": 2, "thystame": 0},
    4: {"players": 4, "linemate": 1, "deraumere": 1, "sibur": 2, "mendiane": 0, "phiras": 1, "thystame": 0},
    5: {"players": 4, "linemate": 1, "deraumere": 2, "sibur": 1, "mendiane": 3, "phiras": 0, "thystame": 0},
    6: {"players": 6, "linemate": 1, "deraumere": 2, "sibur": 3, "mendiane": 0, "phiras": 1, "thystame": 0},
    7: {"players": 6, "linemate": 2, "deraumere": 2, "sibur": 2, "mendiane": 2, "phiras": 2, "thystame": 1},
}


class Player:
    # seuil de food en dessous duquel le joueur cherche à manger en priorité.
    # Relevé à 18 : les phases de convergence (homing vers le groupe) et
    # d'incantation (jusqu'à 3 s bloquées) consomment beaucoup sans manger.
    # Un buffer épais (18 food ≈ 22 s à f=100) évite que l'équipe descende
    # sous le quorum à cause de morts de faim — la survie de TOUS les joueurs
    # est la condition pour atteindre les niveaux 6-7-8 (qui exigent 6 joueurs).
    # Au démarrage (food=10 < 18) le joueur fait d'abord le plein, c'est voulu.
    FOOD_THRESHOLD = 18
    # Seuil critique : en dessous, un joueur prêt à incanter quitte le ralliement
    # pour manger (au-dessus, il reste sur place pour servir de point de rendez-
    # vous stable). 12 food ≈ 15 s à f=100 : marge pour aller manger et revenir.
    CRITICAL_FOOD = 12

    def __init__(self, initial_team_size: int = 1):
        # taille initiale de la team passée au démarrage (--team-size)
        self.initial_team_size = initial_team_size
        # niveau actuel du joueur — démarre à 1, objectif 8
        self.level = 1
        # inventaire : toutes les ressources à 0 au départ
        self.inventory = {resource: 0 for resource in RESOURCES}
        # compteur d'errance, sert à n'émettre l'appel au rassemblement
        # que périodiquement plutôt qu'à chaque tour
        self.wander_tick = 0
        # compteur de pas sans trouver de ressource — utilisé par explore()
        self.explore_steps = 0
        # timestamp jusqu'auquel on ne peut pas forker (cooldown temps-réel).
        # Initialisé à un petit délai aléatoire : staggere le fork initial pour
        # que UN SEUL joueur forke le 6e (les autres reçoivent son broadcast FORK
        # et voient known_team_size==6 avant d'agir → pas de thundering herd).
        self.fork_until = random.uniform(0.0, 3.0)
        # nb de coéquipiers ayant confirmé leur arrivée (HERE) avant l'incantation
        self.here_count = 0
        # estimation du nombre de joueurs dans la team — utilisé uniquement pour
        # les broadcasts FORK (évite que deux instances forkent en même temps)
        self.known_team_size = initial_team_size
        # True quand le joueur a ses pierres et attend des partenaires pour incanter.
        # Permet d'appliquer la logique ancre/suiveur (anti-oscillation).
        self.is_elevating_wait = False
        # Rôle dans le rassemblement : ancre (True) = reste sur place et diffuse
        # des NEED, suiveur (False) = répond aux NEED et se dirige vers les ancres.
        # Décidé aléatoirement à chaque entrée en elevate_wait.
        self.is_anchor = False
        # Nombre de partenaires sur notre tuile (mis à jour à chaque Look dans
        # elevate). Permet d'éviter de quitter un groupe partiel déjà formé.
        self.partners_here = 0
        # Backoff aléatoire (en ticks) avant d'être éligible au fork de croissance.
        # Brise la symétrie : sans lui, tous les joueurs voient known_team_size<6
        # au même tick et forkent simultanément (thundering herd → équipe x2).
        # Le 1er éligible forke, son broadcast FORK met les autres en cooldown.
        self.fork_backoff = random.randint(0, 40)
        # Poids aléatoire fixe pour l'élection d'ancre de rassemblement.
        # À chaque niveau, le joueur du plus haut poids reste immobile (ancre) et
        # tous les autres du même niveau convergent vers lui (broadcasts globaux).
        # Évite l'oscillation des groupes de taille égale qui se croisent sans
        # jamais se poser sur la même tuile.
        self.rally_weight = random.randint(1, 1_000_000_000)
        # Compteur de patience une fois le quorum minimum atteint : on attend des
        # joueurs supplémentaires (objectif : toute l'équipe) avant d'incanter,
        # pour ne pas abandonner les retardataires et garder l'équipe groupée.
        self.quorum_wait = 0

    def update_inventory(self, raw: str):
        # on enlève les crochets et les espaces autour
        raw = raw.strip("[] \n")
        # on découpe par virgule pour avoir chaque "nom quantité"
        for item in raw.split(","):
            parts = item.strip().split()
            # chaque item doit avoir exactement un nom et une quantité
            if len(parts) == 2:
                name, qty = parts
                if name in self.inventory:
                    self.inventory[name] = int(qty)

    def missing_for_elevation(self) -> dict:
        # si on est déjà au niveau max, rien ne manque
        if self.level >= 8:
            return {}
        # on récupère les prérequis du niveau actuel
        req = ELEVATION_REQ[self.level]
        # on retourne uniquement les ressources dont on manque
        # (on ignore food, elle ne sert pas à l'élévation)
        return {
            resource: req[resource] - self.inventory[resource]
            for resource in RESOURCES
            if resource != "food" and self.inventory[resource] < req.get(resource, 0)
        }

    def needs_food(self) -> bool:
        # retourne True si le stock de food est sous le seuil de survie
        return self.inventory["food"] < self.FOOD_THRESHOLD
