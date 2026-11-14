# Zappy - Trantor

A multiplayer network game in which AI-driven players gather resources and perform
elevation rituals on a toroidal map. The game server, the AI clients and the 3D GUI
talk to each other over TCP.

Teams of players start at level 1 and try to reach level 8. To level up, enough
players of the same level must stand on one tile together with the right stones,
then run an incantation. Players also need food to stay alive: each unit of food
lasts 126 time units.

## Architecture

```
            TCP (AI protocol)                 TCP (GUI protocol)
 zappy_ai  ───────────────────►  zappy_server  ◄─────────────────  zappy_gui
 (Python, one process per player) (C++17)                          (C++/SFML/OpenGL)
```

| Component | Folder | Language | Role |
|---|---|---|---|
| Server | `server/` | C++17 | Owns the world: map, tiles, resources, players, eggs. Accepts AI and GUI clients on a single `poll()` loop, queues each player's commands (up to 10 pending), runs them on a game clock driven by the frequency `-f`, handles food consumption and death, incantations, broadcasts and forks, and pushes events to connected GUIs. |
| AI | `ai/` | Python 3 (standard library only) | Autonomous client that drives one player. The first process of a team acts as a "queen" that keeps the team populated with `Fork` and child processes, then settles on one tile; workers stock food, bring her the stones for the whole climb, and the team climbs from level 2 to level 8 together on her tile. See [ai/DOCUMENTATION.md](ai/DOCUMENTATION.md). |
| GUI | `gui/` | C++17, SFML, OpenGL (GLEW, GLM) | 3D viewer: connects as a graphical client, receives the map and live events, and renders islands, players, resources and incantation effects, with a menu, HUD and day/night sky. |

Server source layout: `World` (game rules and command handlers), `Map` / `Tile`
(toroidal grid and resources), `Player`, `Client` (socket buffers), `Server`
(network loop), `Args` (command line).

## Build

Requirements: `g++` with C++17, `make`, Python 3. The GUI also needs SFML 2.x,
GLEW, GLM and OpenGL, plus two header-only libraries (nlohmann/json, tinygltf)
that `gui/scripts/setup_libs.sh` installs into `~/.local/include`.

```bash
make            # server + gui + check that ai/zappy_ai exists
make server     # server only  -> server/zappy_server
make gui        # GUI only     -> gui/zappy_gui
make clean | fclean | re
```

The AI needs no build step.

## Run

**Server**

```bash
./server/zappy_server -p port -x width -y height -n name1 name2 ... -c clientsNb -f freq
```

| Flag | Meaning |
|---|---|
| `-p` | TCP port (1-65535) |
| `-x`, `-y` | map width and height (clamped to 42) |
| `-n` | team names |
| `-c` | initial slots per team |
| `-f` | frequency: time units per second (clamped to 500) |

**AI** (one player; the first process per team spawns its teammates)

```bash
cd ai && python3 zappy_ai -p 4242 -n team1 -h localhost
```

`-p` and `-n` are required, `-h` defaults to `localhost`. Optional environment
variables: `ZAPPY_AI_DELAY` (seconds per move, slows the AI down for viewing) and
`ZAPPY_DEBUG=1` (prints all traffic), `ZAPPY_STRATEGY=legacy` (previous AI).

**GUI**

```bash
cd gui && ./zappy_gui -p 4242 -h localhost
```

The GUI plays an intro, then joins the game when you press START
(`ZAPPY_AUTOSTART` skips this). 3D assets are loaded from `gui/assets/` if present,
otherwise downloaded from a CDN (`ZAPPY_CDN_URL` overrides the address).

**Demo**

```bash
./run_demo.sh [MOVE_SEC] [PORT]   # server (20x20, 2 teams, -c 9, -f 100) + one AI per team + GUI
./test_fork.sh [PORT]             # 4 teams with one slot each, to watch Fork grow the teams
```

## AI results

The AI reliably wins: a team gets 6 players to level 8. Measured with
[`tools/benchmark.py`](tools/benchmark.py) on 16 headless games (server at `-f 100`, 9 slots per team):

| Games | Runs | Result |
| --- | --- | --- |
| previous AI vs itself, 20×20 | 5 | 0 wins after 30,000 time units, stuck at level 5 or 6 |
| current AI vs itself, 20×20 | 5 | 5 wins, after 6,800 to 8,060 time units |
| current AI vs previous AI, 20×20 | 3 | 3 wins for the current AI |
| 4 teams of the current AI, 10×10 | 3 | 3 wins, after 6,730 to 7,260 time units |

The previous AI climbed by pairs and the team split into groups of different levels that could never gather
the 4 or 6 players of the higher levels. The current one keeps the team on one tile from level 2 to 8. Details
and limits in [ai/DOCUMENTATION.md](ai/DOCUMENTATION.md#7-results-and-limits).

```bash
make server
python3 tools/benchmark.py --teams red,blue    # headless game, prints levels until a team wins
```

## Protocol

Both protocols are documented in [`protocol/`](protocol/):

- [`ai_protocol.md`](protocol/ai_protocol.md): handshake, player commands, time costs, asynchronous messages
- [`gui_protocol.md`](protocol/gui_protocol.md): graphical client commands and server events
- [`commands.md`](protocol/commands.md): one-page cheat sheet

## Team

Built by a team of six.

**My contribution (Ridwane777):** the game server core - World/Map/Tile/Player model,
command queue and game clock (time unit), resource consumption and player death,
incantation (two-phase synchronization), sound broadcast direction, and GUI protocol
event broadcasting.

**After the school project (Ridwane777):** the pack strategy of the AI (`ai/logic/pack.py`), which made it
win, and the benchmark tool (`tools/benchmark.py`). The previous AI is kept as `ZAPPY_STRATEGY=legacy`.

Teammates:

- Espoir Biaou ([@Knight-coder-19](https://github.com/Knight-coder-19))
- Jean-Daniel Kangni ([@Black_nexus06](https://github.com/Black_nexus06))
- Chabda Gbemetonou ([@chabda01](https://github.com/chabda01))
- Huziane Tonawa
- Peniel Amoussou

## License

[MIT](LICENSE)
