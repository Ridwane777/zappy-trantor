from network.client import Client
from network.parser import parse_look
from logic.player import Player, ELEVATION_REQ

STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def _topup_stones(client: Client, player: Player, first_tile: list):
    """Drop on the tile ONLY what is missing to meet the requirements.
    Stone pooling: before incanting, check what is ALREADY on the tile
    and only cover the deficit, within what we carry.
    As a result every player present contributes in turn and nothing is over-dropped, so nothing is wasted."""
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        deficit = req.get(stone, 0) - first_tile.count(stone)
        held = player.inventory.get(stone, 0)
        # Drop the missing stones we actually hold, one by one.
        for _ in range(min(deficit, held)):
            client.send(f"Set {stone}")
            player.inventory[stone] -= 1  # keep the local inventory consistent


def _do_incantation(client: Client, player: Player):
    """Top up the missing stones on the tile, then start the incantation.
    Server side: "Incantation" freezes the participants for 300 ticks,
    then returns "Current level: N" on success or "ko" if the conditions are
    no longer met, e.g. a missing player or stone."""
    # Re-read the tile and cover the stone deficit before incanting.
    raw = client.send("Look")
    tiles = parse_look(raw)
    _topup_stones(client, player, tiles[0] if tiles else [])

    # Phase 1: start. "ko" => conditions not met, give up.
    start_msg = client.send("Incantation", is_incantation=True)
    if start_msg == "ko":
        return

    # Phase 2: wait for the final result (300 ticks later).
    end_msg = client.receive(is_incantation=True)
    if end_msg.startswith("Current level"):
        try:
            player.level = int(end_msg.split(":")[1].strip())
        except (IndexError, ValueError):
            pass


def elevate(client: Client, player: Player):
    """Try to raise the player by one level.
    Two cases: the "solo" level, where we incant immediately, and
    group levels, where we wait for the player quorum on the tile, eating
    the food there to hold on, then incant as soon as it is reached.
    """
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    # Level 1 -> 2: one player is enough, incant without waiting.
    if needed == 1:
        _do_incantation(client, player)
        return

    """Look around to count the players on the tile."""
    raw = client.send("Look")
    tiles = parse_look(raw)
    """Other players on the tile, then the total including us."""
    players_here = max(0, tiles[0].count("player") - 1) if tiles else 0
    here = players_here + 1

    """Remember how many players are on the tile;
    the brain uses it as a PROGRESS signal for its patience."""
    player.here_count = here

    """Eat the food on the tile."""
    if tiles:
        for _ in range(tiles[0].count("food")):
            client.send("Take food")

    """Quorum reached: incant for all same-level players on the tile."""
    if here >= needed:
        _do_incantation(client, player)
        return
    