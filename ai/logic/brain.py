from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate
from network.broadcast import decode, encode
from logic.fork import TeamManager
from logic.player import ELEVATION_REQ

"""Decision loop of a player: every turn, observe then act."""
DIRECTION_MOVES = {
    0: [], # already on the sender's tile
    1: ["Forward"], # front
    2: ["Right", "Forward"], # front-right
    3: ["Right", "Forward"], # right
    4: ["Right", "Right", "Forward"], # back-right
    5: ["Right", "Right", "Forward"], # back
    6: ["Left", "Left", "Forward"], # back-left
    7: ["Left", "Forward"], # left
    8: ["Left", "Forward"], # front-left
}

"""Food threshold for the queen."""
QUEEN_FOOD_MIN = 30


class Brain:
    """Decision loop of a player: every turn, observe then act.
    A turn always goes the same way:
    1. the queen (if this is the recruiter process) maintains the headcount;
    2. listen to sounds (broadcasts) to decide on regrouping;
    3. re-read level + inventory from the server;
    4. pick ONE action: eat, collect, regroup, or incant."""
    
    def __init__(self, client: Client):
        self.client = client
        self.player = Player()

    def _handle_broadcasts(self):
        """Listen to the team's sounds and decide on regrouping (anchor/follower).
        Regrouping uses an ELECTED ANCHOR: every player continuously broadcasts "NEED:team:level:weight"
        and the weight tells who to move towards. Since the weight is a FIXED random draw,
        the anchor of a level is unique and stable: everyone converges on the same player without oscillating."""

        # best_weight = the highest weight heard so far (starting from our own).
        # best_dir = direction of the player carrying that highest weight:
        #   None -> nobody stronger heard (so WE are the anchor)
        #   0    -> that player is on OUR tile (we are already at the rally point)
        #   1..8 -> direction to move in to join them
        best_dir = None
        best_weight = self.player.rally_weight

        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if parsed is None:
                continue
            direction, msg_type, data = parsed
            if msg_type != "NEED":
                continue

            # data = "team:level:weight"
            parts = data.split(":")
            try:
                sender_level = int(parts[1])
                sender_weight = int(parts[2])
            except (ValueError, IndexError):
                continue
            # Only listen to players of OUR level: they are the only ones we can incant with.
            if sender_level != self.player.level:
                continue

            # Keep the highest weight, EVEN if it comes from our own tile
            # (direction 0). This is essential: a follower already at the rally point
            # hears the anchor in direction 0 and must STAY there. Otherwise it
            # would leave for the 2nd highest weight still far away.
            if sender_weight > best_weight:
                best_weight = sender_weight
                best_dir = direction
        # If no weight higher than ours was heard, we are the anchor.
        self.player.is_anchor = (best_dir is None)

        """Only move towards the anchor if ready (stones + food) and NOT yet locked."""
        ready = (
            not self.player.missing_for_elevation()
            and not self.player.need_fuel
            and ELEVATION_REQ.get(self.player.level, {}).get("players", 1) > 1
        )

        """Lock the player on the anchor's tile, but unlock if a better anchor is heard."""
        if ready and (self.player.is_anchor or best_dir == 0):
            self.player.locked = True
        elif ready and best_dir:
            self.player.locked = False
        # Only move towards the anchor if ready and NOT yet locked.
        if best_dir and ready and not self.player.locked:
            for move in DIRECTION_MOVES.get(best_dir, []):
                self.client.send(move)

    def run(self):
        """Main loop: one game turn per iteration, up to level 8."""

        """Create a TeamManager to manage the team headcount (the queen) and tick it every turn."""
        team = TeamManager(self.client)
        prev_level = self.player.level

        while True:
            team.tick()

            """The queen never moves: she stays on her tile, only eats when her reserve is low
            and never collects stones (she does not play, she supervises)."""
            if team.is_recruiter:
                raw = self.client.send("Inventory")
                self.player.update_inventory(raw)
                if self.player.inventory["food"] < QUEEN_FOOD_MIN:
                    survive(self.client, self.player)
                continue

            # Listen to sounds and, if needed, move towards the anchor.
            self._handle_broadcasts()

            # Check whether the level changed (successful incantation) and update the player's level
            lvl = self.client.get_level_update()
            if lvl is not None:
                self.player.level = lvl

            # Re-read the inventory (source of truth for food and stones).
            raw = self.client.send("Inventory")
            self.player.update_inventory(raw)

            """Release the lock if the level changed and reset the locked-turns counter."""
            if self.player.level != prev_level:
                self.player.locked = False
                self.player.locked_ticks = 0
                self.player.rally_seen = 0
                prev_level = self.player.level

            """Do not leave the game at level 8: stay connected to count towards victory."""
            if self.player.level >= 8:
                if self.player.inventory["food"] < QUEEN_FOOD_MIN:
                    survive(self.client, self.player)
                continue

            missing = self.player.missing_for_elevation()
            req = ELEVATION_REQ.get(self.player.level, {})
            multi = req.get("players", 1) > 1  # whether this level is a group or solo level
            food = self.player.inventory["food"]

            """The call every player sends each turn so it can be found."""
            if multi:
                self.client.send(
                    f"Broadcast {encode('NEED', f'{self.client.team}:{self.player.level}:{self.player.rally_weight}')}"
                )

            """Never move towards the anchor when critically starving;
            even the anchor goes for food."""
            if food < self.player.CRITICAL_FOOD:
                self.player.need_fuel = True
            elif food >= self.player.FOOD_RALLY_TARGET:
                self.player.need_fuel = False

            """When locked, check whether the number of players on the tile grew since locking:
            if so reset the turn counter, otherwise increment it."""
            if self.player.locked:
                if self.player.here_count > self.player.rally_seen:
                    self.player.rally_seen = self.player.here_count
                    self.player.locked_ticks = 0
                else:
                    self.player.locked_ticks += 1
                    if self.player.locked_ticks >= self.player.LOCK_TIMEOUT:
                        self.player.locked = False
                        self.player.locked_ticks = 0
                        self.player.rally_seen = 0
                        self.player.need_fuel = True
            else:
                self.player.locked_ticks = 0
                self.player.rally_seen = 0

            if multi and self.player.locked:
                """On the anchor's tile and ready: wait for the quorum and incant as soon as it is reached."""
                elevate(self.client, self.player)
            elif multi and self.player.is_anchor:
                """Once settled, the anchor never moves unless critically starving,
                in which case it goes looking for food."""
                if food < self.player.CRITICAL_FOOD:
                    survive(self.client, self.player)
                else:
                    elevate(self.client, self.player)
            else:
                # Follower, or player on a "solo" level (level 1).
                if (not missing) and multi:
                    # Stones already gathered: waiting at the rally point. Only
                    # leave to eat if the reserve is really low (hysteresis)
                    # so the regrouping is not broken.
                    must_eat = self.player.need_fuel
                else:
                    # Collecting (or solo level): keep the normal food buffer.
                    must_eat = self.player.needs_food()

                if must_eat:
                    survive(self.client, self.player)
                elif missing:
                    collect(self.client, self.player)
                else:
                    elevate(self.client, self.player)
