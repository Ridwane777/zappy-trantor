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
    # seuil de food en dessous duquel le joueur doit aller chercher à manger en
    # priorité. On garde une marge de survie mais sans tout monopoliser : sinon,
    # à plusieurs joueurs, la nourriture se raréfie et personne ne s'élève jamais.
    FOOD_THRESHOLD = 12

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
        # cooldown entre deux forks pour ne pas spammer (décrémenté dans brain.run)
        self.fork_cooldown = 0
        # nb de coéquipiers ayant confirmé leur arrivée (HERE) avant l'incantation
        self.here_count = 0

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
