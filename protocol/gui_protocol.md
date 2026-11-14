# GUI (GRAPHIC) protocol - server ⇄ graphic client

Reference for the messages exchanged with a **GRAPHIC** client (the GUI). This
documents what the server in this repo **actually implements**: every message is
built in `server/src/app/GuiProtocol.cpp` and sent by `server/src/app/GameSession.cpp`.

Resource indices everywhere are: `0 food · 1 linemate · 2 deraumere · 3 sibur ·
4 mendiane · 5 phiras · 6 thystame`. `#id` is a numeric id with a leading `#`.
Orientation `O` is `1` north, `2` east, `3` south, `4` west.

## Handshake

```
server → WELCOME\n
client → GRAPHIC\n
server → (bootstrap, see below)
```

On `GRAPHIC` the server immediately sends the world bootstrap:

| order | message | meaning |
|---|---|---|
| 1 | `msz X Y\n` | map size |
| 2 | `tna N\n` (one per team) | team names |
| 3 | `bct X Y q0 q1 q2 q3 q4 q5 q6\n` (every tile) | tile contents |
| 4 | `pnw #id X Y O L team\n` (every living player) | current players |
| 5 | `sgt T\n` | time unit (frequency) |

## Server → GUI events

| message | format | when |
|---|---|---|
| `pnw` | `pnw #id X Y O L team` | a player joined (initial slot or hatched egg) |
| `ppo` | `ppo #id X Y O` | a player moved, turned, or was ejected |
| `plv` | `plv #id L` | a player's level changed (elevation) |
| `pin` | `pin #id X Y q0..q6` | a player's inventory changed (take, drop, eating) |
| `pgt` | `pgt #id i` | player took resource `i` off its tile |
| `pdr` | `pdr #id i` | player dropped resource `i` on its tile |
| `bct` | `bct X Y q0..q6` | a tile's content changed |
| `pex` | `pex #id` | player was ejected (followed by its `ppo`) |
| `pbc` | `pbc #id message` | player broadcast `message` |
| `pic` | `pic X Y L #id #id …` | incantation begins on a tile (L = level, ids = participants) |
| `pie` | `pie X Y R` | incantation ends (R = 1 success, 0 failure) |
| `pfk` | `pfk #id` | player laid an egg (followed by `enw`) |
| `enw` | `enw #egg #player X Y` | new egg on tile X Y, laid by #player |
| `ebo` | `ebo #egg` | a client connected through this egg (followed by `pnw`) |
| `edi` | `edi #egg` | egg destroyed (ejected) |
| `pdi` | `pdi #id` | player died or disconnected |
| `seg` | `seg team` | end of game: `team` has 6 players at level 8 |
| `smg` | `smg message` | message from the server (time unit change, end of game) |

## GUI → server requests

| request | reply | errors |
|---|---|---|
| `msz` | `msz X Y` | |
| `bct X Y` | `bct X Y q0..q6` | `sbp` if X/Y is missing, not a number or outside the map |
| `mct` | one `bct` per tile | |
| `tna` | one `tna N` per team | |
| `ppo #id` | `ppo #id X Y O` | `sbp` if the id is malformed or unknown |
| `plv #id` | `plv #id L` | `sbp` if the id is malformed or unknown |
| `pin #id` | `pin #id X Y q0..q6` | `sbp` if the id is malformed or unknown |
| `sgt` | `sgt T` | |
| `sst T` | `sst T`, then `smg time unit set to T` to every GUI | `sbp` if T is not a number between 1 and 10000 |

An unknown request gets `suc`. A request with the wrong number of parameters gets
`sbp`. Every `sbp` is also logged on the server's standard error.
