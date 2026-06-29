import random

# Les 7 ressources du jeu. "food" sert uniquement à survivre (elle se consomme
# avec le temps) ; les 6 autres ("les pierres") servent aux incantations qui font
# monter de niveau.
RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

# Table des prérequis d'élévation, recopiée du sujet Zappy.
# Pour passer du niveau N au niveau N+1, il faut réunir sur UNE MÊME case :
#   - "players" joueurs du niveau N (qui lancent l'incantation ensemble),
#   - les quantités de pierres indiquées.
# Exemple : passer du niveau 4 au niveau 5 exige 4 joueurs + 1 linemate + 1
# deraumere + 2 sibur + 1 phiras présents sur la case.
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
    """Représente l'état local d'UN joueur, tel que l'IA le connaît.

    Le serveur est la seule source de vérité (position, niveau réel, etc.) ; cette
    classe ne stocke que ce dont l'IA a besoin pour décider quoi faire au tour
    suivant : son niveau, son inventaire, et quelques drapeaux de coordination.
    """

    # --- Seuils de gestion de la nourriture ---
    # En dessous de ce seuil, un joueur en phase de collecte va manger en priorité.
    # 18 food ≈ 22 secondes de vie à fréquence 100 : un coussin confortable, car la
    # convergence vers le point de rendez-vous et les incantations (jusqu'à 300
    # ticks) consomment beaucoup de temps sans permettre de manger.
    FOOD_THRESHOLD = 18
    # Seuil "critique" : c'est seulement en dessous qu'un joueur déjà arrivé au
    # rendez-vous (ou l'ancre, qui doit rester immobile) accepte de partir manger.
    # Au-dessus, il reste sur place pour ne pas casser le regroupement.
    CRITICAL_FOOD = 12
    # Niveau de food visé quand on refait le plein avant/pendant un regroupement.
    # Volontairement modéré : assez pour tenir au rendez-vous sans famine, mais pas
    # trop, sinon le joueur passe son temps à manger au lieu de monter de niveau.
    FOOD_RALLY_TARGET = 25
    # Patience max (en tours de boucle) qu'un joueur VERROUILLÉ accorde au rendez-
    # vous avant d'abandonner : si le quorum ne se forme pas dans ce délai, il se
    # déverrouille, repart se ravitailler, puis retentera. Évite qu'un joueur reste
    # figé pour toujours sur un groupe qui ne se complète jamais. ~60 tours ≈ 15 s.
    LOCK_TIMEOUT = 60

    def __init__(self):
        # Niveau courant : démarre à 1, objectif 8.
        self.level = 1
        # Inventaire local (toutes les ressources à 0 au départ). Mis à jour à
        # chaque tour depuis la réponse de la commande "Inventory" du serveur.
        self.inventory = {resource: 0 for resource in RESOURCES}

        # Compteur de pas effectués en exploration sans rien trouver. Sert à faire
        # tourner le joueur régulièrement (voir explore.py) pour qu'il ne traverse
        # pas la carte en ligne droite et balaie mieux le terrain.
        self.explore_steps = 0

        # Hystérésis de ravitaillement (anti-oscillation) : True = "je refais le
        # plein de food". On passe à True sous CRITICAL_FOOD et on ne repasse à
        # False qu'une fois FOOD_RALLY_TARGET atteint. Entre les deux on garde
        # l'état précédent → un joueur engagé au rendez-vous n'y repart pas manger
        # dès qu'il perd 1 food (ce qui ferait osciller forage <-> rendez-vous).
        # Démarre à True (on commence avec 10 food, donc on mange d'abord).
        self.need_fuel = True

        # Rôle dans le regroupement, recalculé à chaque tour dans le brain :
        #   True  = je suis l'ANCRE de mon niveau (le plus haut "poids" entendu) →
        #           je reste immobile et je sers de point de rendez-vous.
        #   False = je suis un suiveur → je me dirige vers l'ancre.
        self.is_anchor = False

        # VERROUILLAGE au rendez-vous (expérience co-localisation). Une fois prêt ET
        # arrivé sur la case de l'ancre (ou si je SUIS l'ancre), je me verrouille :
        #   - je ne bouge plus ;
        #   - je ne repars JAMAIS manger (je ne consomme que la food de ma case).
        # But : empêcher l'orbite autour de l'ancre et les départs pour cause de
        # faim qui empêchaient 6 joueurs de tenir ensemble sur la même case. Le
        # verrou est levé à chaque changement de niveau (il faut re-collecter pour
        # le niveau suivant) ou si une MEILLEURE ancre est entendue ailleurs.
        self.locked = False

        # Nombre de tours consécutifs passés VERROUILLÉ SANS PROGRÈS (sans nouvelle
        # arrivée sur la case). Sert au déverrouillage de sécurité (voir LOCK_TIMEOUT)
        # avec patience PROGRESSIVE : le compteur se remet à zéro dès qu'un joueur de
        # plus arrive (le groupe grossit) ; on n'abandonne que si plus personne
        # n'arrive depuis LOCK_TIMEOUT tours. Ça distingue "groupe qui se complète
        # lentement" (on attend) de "rendez-vous mort" (on abandonne).
        self.locked_ticks = 0
        # Nombre de joueurs vu sur la case au dernier Look (rempli par elevate) et
        # plus grand nombre vu depuis le verrouillage courant (filigrane de progrès).
        self.here_count = 0
        self.rally_seen = 0

        # "Poids" de ralliement : un grand nombre aléatoire FIXE, tiré une fois.
        # À chaque niveau, le joueur ayant le plus grand poids (toutes équipes
        # confondues) devient l'ancre ; tous les autres convergent vers lui. Ce
        # tirage aléatoire fixe garantit un attracteur unique et stable, ce qui
        # évite que des groupes de taille égale se croisent sans jamais se poser
        # sur la même case.
        self.rally_weight = random.randint(1, 1_000_000_000)

    def update_inventory(self, raw: str):
        """Met à jour l'inventaire local depuis la réponse du serveur.

        Le serveur renvoie l'inventaire sous la forme :
            [food 12, linemate 3, deraumere 0, ...]
        On enlève les crochets, on découpe sur les virgules, puis chaque morceau
        est un couple "nom quantité" qu'on range dans self.inventory.
        """
        raw = raw.strip("[] \n")
        for item in raw.split(","):
            parts = item.strip().split()
            if len(parts) == 2:
                name, qty = parts
                if name in self.inventory:
                    self.inventory[name] = int(qty)

    def missing_for_elevation(self) -> dict:
        """Renvoie les pierres qui MANQUENT pour l'élévation du niveau COURANT.

        On compare les prérequis du niveau actuel à l'inventaire et on ne renvoie
        que les ressources en déficit (la food est ignorée : elle ne sert pas à
        l'élévation). Un dictionnaire vide signifie "j'ai déjà toutes mes pierres,
        je peux aller au rendez-vous".
        """
        if self.level >= 8:
            return {}
        req = ELEVATION_REQ[self.level]
        return {
            resource: req[resource] - self.inventory[resource]
            for resource in RESOURCES
            if resource != "food" and self.inventory[resource] < req.get(resource, 0)
        }

    def needs_food(self) -> bool:
        """True si la réserve de food est passée sous le seuil de survie normal."""
        return self.inventory["food"] < self.FOOD_THRESHOLD
