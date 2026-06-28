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
    # Réserve de food visée avant de rallier (hystérésis). Modérée : assez pour
    # tenir au rendez-vous sans famine, mais pas trop pour ne pas passer tout son
    # temps à manger au lieu de monter (un gros buffer ralentissait la montée).
    FOOD_RALLY_TARGET = 25

    def __init__(self):
        # niveau actuel du joueur — démarre à 1, objectif 8
        self.level = 1
        # inventaire : toutes les ressources à 0 au départ
        self.inventory = {resource: 0 for resource in RESOURCES}
        # compteur d'errance, sert à n'émettre l'appel au rassemblement
        # que périodiquement plutôt qu'à chaque tour
        self.wander_tick = 0
        # compteur de pas sans trouver de ressource — utilisé par explore()
        self.explore_steps = 0
        # timestamp jusqu'auquel on ne peut pas forker (cooldown réel du fork de
        # réapprovisionnement). Petit délai aléatoire initial pour staggerer les
        # joueurs et éviter qu'ils déclenchent un réapprovisionnement au même tick.
        self.fork_until = random.uniform(0.0, 3.0)
        # nb de coéquipiers ayant confirmé leur arrivée (HERE) avant l'incantation
        self.here_count = 0
        # Hystérésis de ravitaillement : True = on (re)fait le plein de food.
        # On bascule à True sous CRITICAL_FOOD et à False à FOOD_RALLY_TARGET.
        # Entre les deux on garde l'état → un joueur engagé au rdv y reste (il ne
        # repart pas manger dès qu'il perd 1 food), ce qui évite l'oscillation
        # forage↔rdv qui empêchait la cohorte de se former. Démarre à True
        # (food=10 < seuil) : on fait le plein avant toute chose.
        self.need_fuel = True
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
        # Poids aléatoire fixe pour l'élection d'ancre de rassemblement.
        # À chaque niveau, le joueur du plus haut poids reste immobile (ancre) et
        # tous les autres du même niveau convergent vers lui (broadcasts globaux).
        # Évite l'oscillation des groupes de taille égale qui se croisent sans
        # jamais se poser sur la même tuile.
        self.rally_weight = random.randint(1, 1_000_000_000)

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

    def missing_stockpile(self) -> dict:
        # STOCKPILE-AHEAD : on vise le CUMUL des pierres nécessaires du niveau
        # courant jusqu'au niveau 8, pas seulement le niveau courant. Ainsi, une
        # fois le stock complet, le joueur n'a plus JAMAIS besoin de repartir
        # collecter entre deux niveaux → il reste sur le rendez-vous et la cohorte
        # ne se redisperse pas. Effet de bord clé : un joueur entièrement stocké
        # est en permanence « ready » → il rallie en continu → le beacon de
        # rassemblement se stabilise et la convergence s'améliore.
        if self.level >= 8:
            return {}
        need = {r: 0 for r in RESOURCES if r != "food"}
        for lvl in range(self.level, 8):
            req = ELEVATION_REQ.get(lvl, {})
            for resource in need:
                need[resource] += req.get(resource, 0)
        return {
            resource: need[resource] - self.inventory[resource]
            for resource in need
            if self.inventory[resource] < need[resource]
        }

    def needs_food(self) -> bool:
        # retourne True si le stock de food est sous le seuil de survie
        return self.inventory["food"] < self.FOOD_THRESHOLD
