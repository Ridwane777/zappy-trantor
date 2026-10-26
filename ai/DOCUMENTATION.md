# Zappy AI documentation (`zappy_ai`)

This document explains **how the AI works** and **what each function does**, file by file.

---

## 1. Overview

`zappy_ai` is the autonomous client that drives **a single player** (a "drone") on the Zappy server. To run a whole team, **several** `zappy_ai` **processes** are started (the first one builds the others through the game's `Fork` command).

**Game goal:** level players up (1 → 8) through **incantations**. An incantation requires, on **the same tile**, a given number of **players of the same level** + a given number of **stones** (see the `ELEVATION_REQ` table).

**How a player works**: an infinite loop that, every turn:
1. manages the team headcount (create/replace teammates);
2. listens to messages (sounds) and heads for the rally point;
3. decides what to do: **eat** (survive), **collect** stones, or **incant**.

**Rule respected:** clients **never communicate outside the game**. All coordination goes through the **server** (sound broadcasts + the `Connect_nbr` command). The only information passed to a child process is its launch role (env variable `ZAPPY_CHILD`), not game data.

---

## 2. Layout

```
ai/
├── zappy_ai                 # entry point (Python executable)
├── network/
│   ├── client.py            # TCP connection + sending/receiving commands
│   ├── parser.py            # parsing of the "Look" answer
│   └── broadcast.py         # encoding/decoding of broadcast messages
└── logic/
    ├── brain.py             # main loop + rallying (homing)
    ├── player.py            # player state + elevation table
    ├── fork.py              # team headcount management (TeamManager)
    ├── explore.py           # exploration moves
    └── states/
        ├── survive.py       # go eat (+ movement helper)
        ├── collect.py       # fetch the missing stones
        └── elevate.py       # gathering + incantation
```

---

## 3. Server protocol (reminder)

**Connection (handshake):**
```
server → WELCOME
client → <team_name>
server → <CLIENT-NUM>      (number of slots still free for the team)
server → <X> <Y>           (map dimensions)
```

**Commands used** (answer in parentheses):
`Forward`, `Right`, `Left` (ok) · `Look` (`[tile0, tile1, ...]`) · `Inventory` (`[food n, linemate n, ...]`) · `Take <object>` / `Set <object>` (ok/ko) · `Broadcast <text>` (ok) · `Fork` (ok) · `Connect_nbr` (a number) · `Incantation` (`Elevation underway` then `Current level: K`, or `ko`).

**Asynchronous messages** (may arrive at any time): `message K, <text>` (broadcast received, `K` = sound direction), `eject: K`, `dead`.

---

## 4. Main loop (summary)

In `Brain.run()` (see [logic/brain.py](logic/brain.py)), every turn:

```
team.tick()                # 1. headcount: create/replace teammates
_handle_broadcasts()       # 2. read sounds -> head for the anchor (homing)
get_level_update()         # 3. pick up a possible level-up
Inventory                  # 4. update the inventory
if level >= 8: stay alive only
                           # 5. decision:
   must eat?  → survive()          (food too low)
   else missing stones? → collect()
   else       → elevate()          (gathering + incantation)
```

---

## 5. Files and functions

### `zappy_ai`: entry point

| Function | Role |
|---|---|
| `parse_args()` | Reads the arguments **`-p` port (required)**, **`-n` team name (required)**, **`-h` host (default `localhost`)**. |
| `main()` | Creates the `Client`, connects, creates the `Brain` and runs `brain.run()`. Catches `ConnectionError`/`BrokenPipeError`/`KeyboardInterrupt` to exit cleanly when the server closes, the player dies, or on Ctrl-C. |

---

### `network/client.py`: network layer (`class Client`)

Handles the TCP socket and command sending/receiving. Buffers broadcasts and level-ups received asynchronously.

**Key attributes:** `host`, `port`, `team`, `map_x/map_y` (map size), `free_slots` (free slots at handshake), `_broadcasts` (queue of received sounds), `_pending_level` (level received asynchronously).

**Environment variables:**
- `ZAPPY_AI_DELAY`: duration of one move in seconds (paces the speed **visible** in the GUI; `0` = full speed, `0.5` ≈ 2 tiles/s).
- `ZAPPY_DEBUG=1`: prints all traffic (`>>` sent, `<<` received).

| Function | Role |
|---|---|
| `connect()` | Opens the socket and performs the handshake (WELCOME → name → CLIENT-NUM → dimensions). Fills `free_slots`, `map_x`, `map_y`. |
| `send(cmd, is_incantation=False)` | Sends a command **and** reads its answer (returns the answer). |
| `receive(is_incantation=False)` | Reads an answer **without** sending (used for the 2nd phase of the incantation). |
| `send_batch(cmds)` | Sends a list of commands in **batches of 10** (server limit: 10 pending requests), then reads all answers. Used for pipelined movement. |
| `get_broadcasts()` | Returns and **empties** the queue of received sounds (consumed by the brain). |
| `get_level_update()` | Returns the level received asynchronously (unsolicited `Current level`, e.g. we took part in an incantation started by someone else), then clears it. |
| `connect_nbr()` | Sends `Connect_nbr` and returns the **number of team eggs** waiting to hatch. Acts as a **semaphore** for `TeamManager` (one egg pending at a time). |
| `_write(msg)` | Writes to the socket. **Only throttles `Forward`** according to `ZAPPY_AI_DELAY` (`Look`/`Inventory`/`Take`… stay fast). |
| `_read_line()` | Reads one line (up to `\n`); raises `ConnectionError` if the server closes. |
| `_read_response(is_incantation)` | Reads a command's answer while **filtering asynchronous messages**: `message …` → stored in `_broadcasts`; `Current level`/`Elevation underway` → returned when waiting for an incantation, otherwise stored/ignored; `eject:` → ignored; `dead` → `ConnectionError`. |

---

### `network/parser.py`

| Function | Role |
|---|---|
| `parse_look(raw)` | Turns the `Look` answer (`"[player food, , linemate, ...]"`) into a **list of tiles**, each tile being a **list of objects** (e.g. `[["player","food"], [], ["linemate"], ...]`). Tile 0 = the player's tile. |

---

### `network/broadcast.py`

Our team messages use the format `TYPE:data` (e.g. `NEED:team1:4:837421`).

| Function | Role |
|---|---|
| `encode(msg_type, data)` | Builds `"TYPE:data"` to send with `Broadcast`. |
| `decode(raw)` | Parses a received `"message K, TYPE:data"` → returns the tuple **`(direction K, TYPE, data)`** (or `None` if it is not a broadcast). `K` is the sound direction (0 = same tile, 1-8 around). |

---

### `logic/player.py`: player state

**`RESOURCES`**: the list of the 7 resources (food + 6 stones).

**`ELEVATION_REQ`**: the **elevation table**: for each current level, the number of **players** required and the quantity of each **stone**:

| Level | players | linemate | deraumere | sibur | mendiane | phiras | thystame |
|---|---|---|---|---|---|---|---|
| 1→2 | 1 | 1 | 0 | 0 | 0 | 0 | 0 |
| 2→3 | 2 | 1 | 1 | 1 | 0 | 0 | 0 |
| 3→4 | 2 | 2 | 0 | 1 | 0 | 2 | 0 |
| 4→5 | 4 | 1 | 1 | 2 | 0 | 1 | 0 |
| 5→6 | 4 | 1 | 2 | 1 | 3 | 0 | 0 |
| 6→7 | 6 | 1 | 2 | 3 | 0 | 1 | 0 |
| 7→8 | 6 | 2 | 2 | 2 | 2 | 2 | 1 |

**Food constants (`class Player`):**
- `FOOD_THRESHOLD = 18`: normal threshold below which a **collecting** player goes to eat.
- `CRITICAL_FOOD = 12`: critical threshold (a player at the rally point only leaves to eat below it).
- `FOOD_RALLY_TARGET = 25`: food level to fill up to (hysteresis) before settling at the rally point.
- `LOCK_TIMEOUT = 60`: max patience (in turns) of a locked player without progress before giving up the rally point.

**Key attributes:** `level`, `inventory`, `need_fuel` (food hysteresis), `is_anchor` (am I the anchor of my level?), `rally_weight` (fixed random weight for anchor election), `explore_steps` (counter for zigzag exploration), `locked` (locked at the rally point?), `locked_ticks` (turns without progress since locking), `here_count` (players currently on my tile), `rally_seen` (max players seen since locking).

| Method | Role |
|---|---|
| `update_inventory(raw)` | Parses the `Inventory` answer and updates `self.inventory`. |
| `missing_for_elevation()` | Returns the stones missing for the **current level only** (ignores food). `{}` if level ≥ 8. This is the only collection target: collect briefly, level up, repeat. |
| `needs_food()` | `True` if `food < FOOD_THRESHOLD`. |

---

### `logic/brain.py`: the brain (`class Brain`)

**`DIRECTION_MOVES`**: table that turns a **sound direction** (tile number K, 1-8, sent by the server) into a **sequence of moves** towards the source:

| K (sound direction) | moves |
|---|---|
| 0 (same tile) | none |
| 1 (front) | `Forward` |
| 2, 3 (right) | `Right, Forward` |
| 4, 5 (back) | `Right, Right, Forward` |
| 6 (back-left) | `Left, Left, Forward` |
| 7, 8 (left) | `Left, Forward` |

| Method | Role |
|---|---|
| `__init__(client)` | Creates the `Player`. |
| `_handle_broadcasts()` | **Rallying (homing).** Reads every received sound. For `NEED` messages **of our level** (all teams included, since an incantation gathers same-level players whatever their team), finds the **highest `weight`** heard (anchor election). If someone has a higher weight than ours, we are a **follower** → we head for them. Otherwise **we are the anchor** (`is_anchor = True`). The highest weight is kept **even in direction 0** (our tile) → a follower that already arrived **stays** instead of being pulled elsewhere. We only move when **`ready`** (nothing left to collect for the current level, food OK, level needs ≥ 2 players). Once ready and at the rally point (anchor or direction 0), we **lock** (`locked = True`). |
| `run()` | **Main loop** (see §4). Creates the `TeamManager`. If this process is the **queen** (`is_recruiter`): it only survives (eats below `QUEEN_FOOD_MIN=30`) + `team.tick()`. Otherwise (worker), every turn: `team.tick()` → `_handle_broadcasts()` → level update → `Inventory` → stay idle at level 8 → broadcast the **`NEED` beacon** (on group levels) → update food hysteresis → **progressive lock patience** → **decision**: locked → `elevate`; anchor → `elevate` (or `survive` when critically starving); otherwise `survive`/`collect`/`elevate`. |

**Decision logic (in `run`)**:
- **Locked** at the rally point (`locked`): no longer moves and never leaves to eat → `elevate()` (incants as soon as the quorum is reached).
- **Anchor** not locked: **settles** as the rally point (`elevate`), and only leaves to eat (`survive`) below `CRITICAL_FOOD`.
- `need_fuel` becomes `True` below `CRITICAL_FOOD`, `False` at `FOOD_RALLY_TARGET`, and **keeps its state in between** (hysteresis) → a follower committed to the rally point does not leave as soon as it loses 1 food.
- **Ready** follower (stones OK, multi-player level): `must_eat = need_fuel`; otherwise `must_eat = needs_food()` (normal threshold). Then `survive` / `collect` / `elevate`.

**Progressive lock patience**: while `locked`, if `here_count > rally_seen` (one more player just arrived) → `locked_ticks = 0` (the group grows, keep waiting). Otherwise `locked_ticks += 1`; after `LOCK_TIMEOUT (60)` turns without progress → give up: `locked = False`, `need_fuel = True`, go refuel and retry later. A level change automatically releases the lock.

---

### `logic/fork.py`: headcount management (`class TeamManager`)

**`TEAM_CAP = 9`**: **hard** cap of players per team (never exceeded). **`REFORK_BELOW = 7`**: replenishment threshold (hysteresis): the queen only re-forks when the headcount drops **below 7**, then rebuilds up to 9 (7 = 1 queen + 6 workers = quorum of levels 6-7). **`BUILD_FORK_COOLDOWN = 2.0`**: delay between two creations (time for an egg to hatch).

| Item | Role |
|---|---|
| `_spawn(client)` | Starts a new `zappy_ai` process (with `-p/-n/-h`) and **returns its handle** (to monitor it). The child gets `ZAPPY_CHILD=1` (just a **launch role**, not game data) → it does not recruit, it plays. On connection, the server gives it a free slot, or hatches an egg. |
| `TeamManager.__init__` | Determines whether this process is the **recruiter** (`is_recruiter` = started by hand, without `ZAPPY_CHILD`). Stores the initial `free_slots`. |
| `TeamManager.tick()` | **Builds and maintains the headcount (hysteresis 7→9).** Only the **recruiter** (the queen) acts. It: (1) **removes** children whose **process died** (= dead player); (2) computes the headcount `1 (queen) + living children`; (3) enters **rebuild** mode when the headcount drops below `REFORK_BELOW` (7) and leaves it at `TEAM_CAP` (9); (4) in rebuild mode, once the cooldown is over: creates a teammate, through a remaining **initial slot**, otherwise an **egg** (`Fork`). **Key point:** the child is only started **when a slot/egg exists** (free slot, or `connect_nbr() > 0` = egg ready); otherwise we `Fork` first and the child comes on a later turn. This keeps children started too early from getting "ko" and **hanging** (process leak). |

> **How the team is replenished:** the recruiter monitors its children's **process handles**. When a player dies, its process ends; `tick()` detects it (`poll()`) and removes it. As soon as the headcount drops **below 7**, the queen rebuilds up to **9** (Fork + new processes). Capped at 9.

---

### `logic/states/survive.py`: eating

| Function | Role |
|---|---|
| `_go_to_tile(client, index)` | **Computes and runs the path** to tile `index` of the field of view (`Look`). Geometry: row `r = floor(√index)`, lateral offset `col = index − r(r+1)` (negative = left). Sends `Forward × r` + one turn + `Forward × |col|` in one batch (`send_batch`). Helper reused by `collect`. |
| `survive(client, player)` | `Look`, goes to the **closest food tile** and picks up **all** the food there. If no food is in sight → `explore()`. |

---

### `logic/states/collect.py`: collecting stones

| Function | Role |
|---|---|
| `collect(client, player)` | Computes the missing stones for the **current level** (`missing_for_elevation`). `Look`, goes to the **first tile** holding a missing stone, picks it up (+ any food there). If nothing is in sight → `explore()`. |

---

### `logic/states/elevate.py`: gathering + incantation

`STONES`: the 6 stones (without food).

| Function | Role |
|---|---|
| `_topup_stones(client, player, first_tile)` | **Pooled drop.** Drops on the tile **only the deficit** needed to meet the level's requirements: counts what is **already** on the tile (dropped by teammates) and only adds what is missing, within what we carry. → everyone contributes, nothing is wasted. |
| `_do_incantation(client, player)` | `Look` + `_topup_stones` (make sure the tile has the stones) + sends `Incantation` (2 phases: `Elevation underway`, then 300 ticks later `Current level: K`). On success, updates `player.level`. |
| `elevate(client, player)` | If the level requires **1 player** → incants directly. Otherwise: `Look`, counts the players present (`here`), **eats the food on the tile** (self-feeding while waiting). If the **quorum is reached** (`here ≥ needed`) → incants. Otherwise → waits in place (the `NEED` beacon is broadcast by `run()` every turn). |

---

### `logic/explore.py`

| Function | Role |
|---|---|
| `explore(client, player)` | Exploration move: moves forward and **turns randomly every 3 steps** so it does not cross the map in a straight line (and covers the sides). |

---

## 6. Key concepts (strategies)

- **Rallying by anchor election.** Each player has a fixed random `rally_weight`. Players on group levels broadcast `NEED:team:level:weight`. The one with the **highest weight** stays still (anchor); the others head for it using the **sound direction** (`DIRECTION_MOVES`). A single stable attractor → convergence without oscillation.

- **Locking at the rally point.** As soon as a player is ready and on the anchor's tile, it locks: it no longer moves and never leaves to eat. Patience is **progressive**: if the group grows (`here_count` increases), the counter resets. If nobody arrives for `LOCK_TIMEOUT (60)` turns → give up + refuel + retry. This avoids orbiting around the anchor and the early departures to eat that kept the quorum from forming.

- **Continuous beacon.** `NEED` is broadcast **every turn**: a sound's direction (tile number) depends on the receiver's **current** position/orientation; an old direction is useless. Broadcasting continuously gives followers a fresh direction at every step.

- **Collecting for the current level only.** Only the current level's stones are collected (`missing_for_elevation()`). Collection stays short → the player quickly joins the rally point, incants, then starts again for the next level.

- **Food hysteresis.** Two thresholds (`CRITICAL_FOOD=12`, `FOOD_RALLY_TARGET=25`): fill up, then **stay** at the rally point until dropping back to the low threshold → avoids the "eat ↔ rally" oscillation.

- **Pooled stone drop.** At incantation time, each player present only drops the **deficit** → nothing is wasted, and the cost is not borne by a single player.

- **Headcount management (TeamManager).** One **recruiter** per team builds the headcount up to **9** and **maintains** it (replaces dead players by monitoring processes), capped at 9. Regulated by the server (`Connect_nbr` = egg semaphore).

- **No out-of-game communication.** No game data is exchanged outside the server. The headcount is driven by the server (handshake slots + `Connect_nbr`). The only info passed to a child process is `ZAPPY_CHILD` (a launch role, not game data).

---

## 7. Results and limits

- **Level 8 reached.** The rally-point lock + progressive patience made it possible to reach level 8 (9 players reached level 8 in a live run with 4 teams, 10×10 map, f=100).
- **Server fix.** Food is no longer consumed during the 300-tick incantation freeze (`isIncanting` in World.cpp) → avoids starvation during the 7→8 ritual.
- **Remaining hard part.** Gathering 6 same-level players on one exact tile using sound (direction accurate to 1 out of 8) remains random. Success depends on map size and run variance.

---

## 8. Running the AI

**One player:**
```bash
cd ai && python3 zappy_ai -p 4242 -n team1 -h localhost
```
*(The first process started is the "recruiter": it builds the rest of the team through `Fork`.)*

**Bundled demos** (from the repository root):
```bash
./run_demo.sh        # server + several teams + GUI
./test_fork.sh       # reproduction (Fork) test: the headcount grows
```
