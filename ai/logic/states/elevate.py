from network.client import Client
from network.parser import parse_look
from logic.player import Player, ELEVATION_REQ
from network.broadcast import encode
from logic.fork import should_fork, do_fork

STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

# Ticks d'attente une fois le quorum MINIMUM atteint, avant d'incanter sans
# attendre toute l'équipe (fallback anti-deadlock si des coéquipiers sont morts).
INCANT_PATIENCE = 30


def _do_incantation(client: Client, player: Player):
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        for _ in range(req.get(stone, 0)):
            client.send(f"Set {stone}")
            
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

    # --- Décision d'incantation (lockstep souple) ---
    # On préfère rassembler toute l'équipe (jusqu'à 6) avant d'incanter, pour ne
    # pas abandonner les retardataires (le serveur élève TOUS les joueurs de même
    # niveau présents → en incanter 6 d'un coup les fait monter ensemble).
    # MAIS le fallback est basé sur wander_tick (temps TOTAL d'attente à ce
    # niveau), pas sur des ticks consécutifs au quorum : sinon un joueur qui part
    # manger réinitialise le compteur et on attend 6 pour toujours alors que
    # l'équipe a rétréci sous 6 (deadlock). Après INCANT_PATIENCE, on incante
    # avec le quorum minimum présent.
    if here >= needed:
        target = max(needed, min(6, player.known_team_size))
        if here >= target or player.wander_tick >= INCANT_PATIENCE:
            player.wander_tick = 0
            _do_incantation(client, player)
            return
    else:
        player.quorum_wait = 0

    player.is_elevating_wait = True
    player.wander_tick += 1

    # NB : pas de pas local via Look ici. Il faisait dériver l'ancre vers ses
    # suiveurs visibles (elle les pourchasse → oscillation, jamais de rendez-
    # vous). Tout déplacement passe par le homing broadcast (brain), où seuls
    # les non-ancres bougent.

    # --- Rassemblement par ancre élue (Broadcast) ---
    # On diffuse NEED:team:level:weight. L'ancre = le joueur de plus haut weight
    # parmi ceux du même niveau ; elle reste immobile, les autres convergent vers
    # elle (voir brain._handle_broadcasts). Broadcasts globaux sur ce serveur.
    if player.wander_tick % 4 == 0:
        client.send(
            f"Broadcast {encode('NEED', f'{client.team}:{player.level}:{player.rally_weight}')}"
        )

    # Fork uniquement pour amener l'équipe à 6 (géré par should_fork).
    if should_fork(client, player):
        do_fork(client, player)
