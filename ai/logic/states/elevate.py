from network.client import Client
from network.parser import parse_look
from logic.player import Player, ELEVATION_REQ
from network.broadcast import encode

STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def _topup_stones(client: Client, player: Player, first_tile: list):
    """Dépose sur la case SEULEMENT le déficit pour atteindre les prérequis du
    niveau (mise en commun). On compte ce qui est DÉJÀ sur la case (posé par les
    coéquipiers) et on n'ajoute que ce qui manque, dans la limite de ce qu'on
    porte. Effet : chaque joueur présent contribue tour à tour, ce ne sont pas
    les ressources d'une seule personne qui sont prises, et rien n'est gaspillé.
    """
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        deficit = req.get(stone, 0) - first_tile.count(stone)
        held = player.inventory.get(stone, 0)
        for _ in range(min(deficit, held)):
            client.send(f"Set {stone}")
            player.inventory[stone] -= 1  # garder l'inventaire local cohérent


def _do_incantation(client: Client, player: Player):
    # On regarde la case et on complète le déficit de pierres (ce que les autres
    # ont déjà déposé compte) avant de lancer l'incantation.
    raw = client.send("Look")
    tiles = parse_look(raw)
    _topup_stones(client, player, tiles[0] if tiles else [])

    start_msg = client.send("Incantation", is_incantation=True)
    if start_msg == "ko":
        return
        
    end_msg = client.receive(is_incantation=True)
    if end_msg.startswith("Current level"):
        try:
            player.level = int(end_msg.split(":")[1].strip())
            client.send(f"Broadcast {encode('RESUME', f'{client.team}:{player.level}')}")
        except (IndexError, ValueError):
            pass


def elevate(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    if needed == 1:
        _do_incantation(client, player)
        return

    raw = client.send("Look")
    tiles = parse_look(raw)
    players_here = max(0, tiles[0].count("player") - 1) if tiles else 0
    player.partners_here = players_here
    here = players_here + 1

    # --- Auto-alimentation sur place ---
    # Un joueur qui attend (surtout l'ancre, immobile) mange la nourriture
    # présente sur sa tuile : il se nourrit sans quitter le rendez-vous, ce qui
    # évite que l'équipe meure de faim pendant les longues attentes/incantations.
    if tiles:
        for _ in range(tiles[0].count("food")):
            client.send("Take food")

    # --- Décision d'incantation ---
    # Dès que le quorum du niveau est atteint sur la tuile, on incante (le serveur
    # élève tous les joueurs de même niveau présents). Attendre les 6 a été testé
    # et n'aide pas : le blocage n'est pas le timing mais la co-localisation de 4+
    # joueurs sur une case. Incanter au quorum maximise la progression observée.
    if here >= needed:
        player.wander_tick = 0
        _do_incantation(client, player)
        return

    player.is_elevating_wait = True
    player.wander_tick += 1

    # --- Rassemblement par ancre élue (Broadcast) ---
    # On diffuse NEED:team:level:weight à CHAQUE tour (beacon continu). C'est
    # essentiel : un suiveur ne peut se diriger que s'il reçoit une direction
    # FRAÎCHE à ce tour (le numéro de tuile K dépend de sa position/orientation
    # courante, qui changent dès qu'il bouge → impossible de réutiliser une
    # ancienne direction). En émettant à chaque tour, le suiveur a une direction à
    # chaque pas et converge vite (vérifié en simulation : ~7-8 pas). L'ancre =
    # plus haut weight ; elle reste immobile, les autres convergent (brain).
    client.send(
        f"Broadcast {encode('NEED', f'{client.team}:{player.level}:{player.rally_weight}')}"
    )
    # (Plus de fork ici : l'effectif est géré par TeamManager — construction à 9
    # + remplacement des morts, plafonné à 9.)
