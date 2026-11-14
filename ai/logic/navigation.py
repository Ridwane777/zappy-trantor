"""Turning positions and sound directions into moves."""
import math
import random

# Moves toward a sound direction: 1 = front, then clockwise (the server's numbering).
# Diagonals take both steps; behind means a half turn.
DIRECTION_MOVES = {
    0: [],
    1: ["Forward"],
    2: ["Forward", "Right", "Forward"],
    3: ["Right", "Forward"],
    4: ["Right", "Right", "Forward"],
    5: ["Right", "Right", "Forward"],
    6: ["Left", "Left", "Forward"],
    7: ["Left", "Forward"],
    8: ["Forward", "Left", "Forward"],
}


def path_to_tile(index: int) -> list:
    """Moves to the `index`-th tile of a Look. Row r of the vision cone holds tiles
    r*r to (r+1)*(r+1)-1, its center tile is r*(r+1)."""
    if index <= 0:
        return []
    row = math.isqrt(index)
    offset = index - row * (row + 1)
    moves = ["Forward"] * row
    if offset:
        moves.append("Right" if offset > 0 else "Left")
        moves.extend(["Forward"] * abs(offset))
    return moves


class Explorer:
    """Zigzag exploration when nothing useful is in sight: forward, and a random
    quarter turn every few steps, which covers the map better than a straight line."""

    def __init__(self, turn_every: int = 3, rng: random.Random = None):
        self._steps = 0
        self._turn_every = turn_every
        self._rng = rng or random.Random()

    def next_moves(self) -> list:
        self._steps += 1
        moves = [self._rng.choice(["Left", "Right"])] if self._steps % self._turn_every == 0 else []
        return moves + ["Forward"]
