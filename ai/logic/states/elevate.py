from network.client import Client
from network.parser import parse_look
from logic.player import Player, ELEVATION_REQ

# Les 6 pierres déposables sur le sol pour une incantation (la food n'en fait pas
# partie). L'ordre n'a pas d'importance, il sert juste à parcourir les pierres.
STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def _topup_stones(client: Client, player: Player, first_tile: list):
    """Dépose sur la case SEULEMENT ce qui manque pour atteindre les prérequis.

    Mise en commun des pierres : avant d'incanter, on regarde ce qui est DÉJÀ sur
    la case (déposé par les coéquipiers arrivés avant nous) et on ne complète que
    le déficit, dans la limite de ce qu'on porte. Conséquences :
      - chaque joueur présent contribue à tour de rôle (ce ne sont pas les pierres
        d'une seule personne qui sont consommées) ;
      - on ne dépose jamais en trop, donc rien n'est gaspillé.

    `first_tile` est le contenu de notre case (la 1re case renvoyée par Look).
    """
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        deficit = req.get(stone, 0) - first_tile.count(stone)
        held = player.inventory.get(stone, 0)
        # On dépose une à une les pierres manquantes qu'on possède réellement.
        for _ in range(min(deficit, held)):
            client.send(f"Set {stone}")
            player.inventory[stone] -= 1  # on garde l'inventaire local cohérent


def _do_incantation(client: Client, player: Player):
    """Complète les pierres manquantes sur la case puis lance l'incantation.

    Déroulé côté serveur : "Incantation" gèle les participants pendant 300 ticks,
    puis renvoie "Current level: N" en cas de succès (ou "ko" si les conditions ne
    sont plus réunies, par exemple s'il manque un joueur ou une pierre).
    """
    # On relit la case et on complète le déficit de pierres avant d'incanter.
    raw = client.send("Look")
    tiles = parse_look(raw)
    _topup_stones(client, player, tiles[0] if tiles else [])

    # Phase 1 : démarrage. "ko" => les conditions ne sont pas réunies, on abandonne.
    start_msg = client.send("Incantation", is_incantation=True)
    if start_msg == "ko":
        return

    # Phase 2 : on attend le résultat final (300 ticks plus tard).
    end_msg = client.receive(is_incantation=True)
    if end_msg.startswith("Current level"):
        try:
            player.level = int(end_msg.split(":")[1].strip())
        except (IndexError, ValueError):
            pass


def elevate(client: Client, player: Player):
    """Tente de faire monter le joueur d'un niveau.

    Deux cas :
      - niveau "solo" (1 seul joueur requis) : on incante immédiatement ;
      - niveau de groupe : on attend d'avoir le quorum de joueurs sur la case, en
        mangeant la food présente pour tenir, puis on incante dès qu'il est atteint.
    Le déplacement vers le point de rendez-vous est géré ailleurs (brain.py) ; ici
    on ne fait que constater la situation sur place et incanter quand c'est bon.
    """
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    # Niveau 1 -> 2 : un seul joueur suffit, on incante sans attendre personne.
    if needed == 1:
        _do_incantation(client, player)
        return

    # Niveau de groupe : on regarde combien de joueurs sont sur notre case.
    raw = client.send("Look")
    tiles = parse_look(raw)
    # tiles[0] = notre propre case. Elle nous inclut nous-même dans le compte des
    # "player", d'où le -1 pour obtenir le nombre de PARTENAIRES, puis +1 pour le
    # total présent (partenaires + nous).
    players_here = max(0, tiles[0].count("player") - 1) if tiles else 0
    here = players_here + 1

    # Auto-alimentation sur place : un joueur qui attend (en particulier l'ancre,
    # immobile) mange la food présente sur sa case. Il se nourrit sans quitter le
    # rendez-vous, ce qui évite que le groupe meure de faim pendant les attentes.
    if tiles:
        for _ in range(tiles[0].count("food")):
            client.send("Take food")

    # Quorum atteint : on incante. Le serveur élève d'un coup tous les joueurs de
    # notre niveau présents sur la case (toutes équipes confondues).
    if here >= needed:
        _do_incantation(client, player)
        return

    # Quorum pas encore atteint : on patiente sur place. Le beacon de regroupement
    # (broadcast "NEED" à chaque tour) est émis par brain.py ; il n'y a donc rien à
    # faire ici, on attend simplement l'arrivée des autres au prochain tour.
