from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.states.survive import _go_to_tile
from logic.explore import explore


def collect(client: Client, player: Player):
    """Fetch the stones missing for the current level's elevation.
    Simple strategy:
      1. compute the missing stones (current level only);
      2. look around (Look);
      3. go to the FIRST visible tile holding a useful stone and pick it
         up (along with any food there);
      4. if nothing useful is visible, explore to discover other tiles.
    Only the current level is targeted (no large stock in advance): collection
    stays short, so the player can quickly go back to regrouping instead
    of spending all its time hoarding stones."""
    missing = player.missing_for_elevation()

    # Current field of view: list of tiles (see parse_look).
    raw = client.send("Look")
    tiles = parse_look(raw)

    # Find the first tile holding at least one missing stone.
    for index, tile in enumerate(tiles):
        resources_here = [r for r in missing if r in tile]
        if resources_here:
            # A single move to that tile.
            _go_to_tile(client, index)
            # Pick up every useful stone on the tile.
            for resource in resources_here:
                client.send(f"Take {resource}")
            # Opportunistic bonus: while here, also take the food.
            # Without it the food reserve never goes above the bare survival threshold.
            for _ in range(tile.count("food")):
                client.send("Take food")
            return
    # No useful stone in sight: explore other areas.
    explore(client, player)
