# AI protocol - server ⇄ AI client

Reference for an **AI** client (a `zappy_ai` bot). Documents what this repo's
server (`server/src/game/World.cpp`, `server/src/app/GameSession.cpp`) and AI
(`ai/`) actually implement.

## Handshake

```
server → WELCOME\n
client → TEAM-NAME\n          (e.g. "team1")
server → CLIENT-NUM\n         (free slots left for that team: initial slots + eggs)
server → X Y\n                (map width and height)
```

If the team is full (no slot, no egg) or unknown, the server replies `ko\n` and
closes the socket. A client that joins through an egg appears on the egg's tile.

## Commands

One command per line, executed in order. A player can have at most **10**
commands waiting: further lines are dropped without reply until one completes.
Each command costs time units (`T`); with the server frequency `f` (`-f`), one
time unit lasts `1/f` second.

| command | cost | reply | effect |
|---|---|---|---|
| `Forward` | 7 | `ok` | move one tile forward (the map wraps: torus) |
| `Right` | 7 | `ok` | turn 90° right |
| `Left` | 7 | `ok` | turn 90° left |
| `Look` | 7 | `[t0,t1,t2,…]` | objects on each visible tile (vision grows with level) |
| `Inventory` | 1 | `[food n, linemate n, …]` | current inventory |
| `Broadcast text` | 7 | `ok` | send `text` to every player (heard as `message K, text`) |
| `Connect_nbr` | 0 | `N` | free team slots (initial slots left + eggs) |
| `Fork` | 42 | `ok` | lay an egg on the current tile (one more slot) |
| `Eject` | 7 | `ok` / `ko` | push players sharing this tile one tile away, destroy eggs here |
| `Take object` | 7 | `ok` / `ko` | pick `object` up from the current tile |
| `Set object` | 7 | `ok` / `ko` | drop `object` on the current tile |
| `Incantation` | 300 | see below | start an elevation ritual |

An unknown command, or an argument given to a command that takes none, is
answered `ko` in its turn, so replies stay in order.

Asynchronous messages, which can arrive between replies:

| message | meaning |
|---|---|
| `message K, text` | a broadcast heard from direction `K` |
| `eject: K` | this player was pushed by an `Eject` coming from direction `K` |
| `Current level: L` | an incantation this player took part in succeeded |
| `dead` | the player starved; the server closes the connection |

Directions: `0` = same tile, `1` = front, then clockwise around the player up to `8`
(front-right is `2`, right is `3`, back is `5`, left is `7`). The sound takes the
shortest way on the torus.

## Food

A player starts with 10 food. One food unit lasts 126 time units; when the last one
runs out, the player dies (`dead`). Food is not consumed while the player is frozen
in an incantation.

## Incantation (elevation)

```
client → Incantation\n
server → Elevation underway\n          (to every participant, if the requirements are met)
server → Current level: K\n            (300 time units later, if they still are)
```

Participants are all players of the incantor's level on the tile; they are frozen
during the ritual. Requirements are in `server/include/game/GameRules.hpp`
(mirrored in `ai/logic/player.py`). If they are not met at the start or at the end,
the server replies `ko`.

`Current level: K` is sent to **every** participant, including the ones that did
not send `Incantation`: the AI treats it as an out-of-band message (buffered in
`ai/network/client.py`) rather than as the reply to its own pending command.
