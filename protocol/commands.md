# Protocol quick reference

One-page cheat sheet for both protocols. Full details: [`ai_protocol.md`](ai_protocol.md),
[`gui_protocol.md`](gui_protocol.md). Resource order everywhere: `0 food · 1 linemate ·
2 deraumere · 3 sibur · 4 mendiane · 5 phiras · 6 thystame`.

## AI client → server

Handshake: `WELCOME` → send team name → `CLIENT-NUM` → `X Y` (or `ko`).

| command | cost | reply |
|---|---|---|
| `Forward` `Right` `Left` | 7 | `ok` |
| `Look` | 7 | `[t0,t1,…]` |
| `Inventory` | 1 | `[food n, linemate n, …]` |
| `Broadcast text` | 7 | `ok` |
| `Connect_nbr` | 0 | `N` |
| `Fork` | 42 | `ok` |
| `Eject` | 7 | `ok` / `ko` |
| `Take object` / `Set object` | 7 | `ok` / `ko` |
| `Incantation` | 300 | `Elevation underway` → `Current level: K` (or `ko`) |
| anything else | 0 | `ko` |

At most 10 pending commands per player. Asynchronous: `message K, text`,
`eject: K`, `Current level: K`, `dead`.

## Server → GUI (GRAPHIC)

Handshake: `WELCOME` → send `GRAPHIC` → bootstrap (`msz`, `tna…`, `bct…`, `pnw…`, `sgt`).

| msg | format | | msg | format |
|---|---|---|---|---|
| `msz` | `msz X Y` | | `pgt` | `pgt #id i` (took) |
| `sgt` | `sgt T` | | `pdr` | `pdr #id i` (dropped) |
| `tna` | `tna N` | | `pic` | `pic X Y L #id…` |
| `bct` | `bct X Y q0..q6` | | `pie` | `pie X Y R` |
| `pnw` | `pnw #id X Y O L team` | | `pbc` | `pbc #id message` |
| `ppo` | `ppo #id X Y O` | | `pfk` | `pfk #id` |
| `plv` | `plv #id L` | | `enw` | `enw #egg #player X Y` |
| `pin` | `pin #id X Y q0..q6` | | `ebo` | `ebo #egg` (hatched) |
| `pdi` | `pdi #id` | | `edi` | `edi #egg` (destroyed) |
| `pex` | `pex #id` | | `seg` | `seg team` |
| `smg` | `smg message` | | `sst` | `sst T` |
| `suc` | unknown request | | `sbp` | bad parameters |

GUI → server requests: `msz`, `bct X Y`, `mct`, `tna`, `ppo #id`, `plv #id`,
`pin #id`, `sgt`, `sst T`.
