# Zappy AI documentation (`zappy_ai`)

How the AI works, how its code is organized, and what was measured. Python 3,
standard library only.

## 1. Overview

`zappy_ai` drives **one player**. A team is several processes: the first one started
becomes the **queen**, which starts the others (the **workers**) through free slots and
`Fork` eggs, and keeps the team at 9 players.

**Goal:** 6 players of one team at level 8. Leveling up needs an incantation: on one
tile, enough players of the same level and the right stones (`logic/rules.py`).

**Rule respected:** clients never communicate outside the game. Everything goes
through the server (broadcasts, `Connect_nbr`). A child process only gets its launch
role (`ZAPPY_CHILD=1`), never game data.

## 2. Layout

```
ai/
├── zappy_ai                 entry point: connects, picks queen or worker
├── network/                 talking to the server
│   ├── client.py            Client: TCP line transport, async events (broadcasts, level-ups, death)
│   ├── game_api.py          GameApi: the only place that writes commands (look, take, incant...)
│   └── protocol.py          pure parsing of replies (Look, Inventory, message, level)
├── comm/                    talking to the team
│   ├── secure.py            SecureChannel: encrypted, authenticated, replay-proof messages
│   ├── messages.py          Home: the queen's message, typed
│   └── jammer.py            Jammer: corrupts and re-broadcasts other teams' messages
├── logic/                   playing
│   ├── rules.py             resources, elevation table, climb_need()
│   ├── navigation.py        sound direction -> moves, Look tile -> path, Explorer
│   ├── player.py            Player: level and inventory, changed only through methods
│   ├── team.py              ProcessSpawner (OS processes) and Recruiter (headcount policy)
│   └── pack.py              PackAgent, PackQueen, PackWorker: the strategy
└── tests/                   31 unit tests (python3 -m unittest discover -s tests -t .)
```

Dependencies only go downward: `logic` uses `comm` and `network.game_api`; the strategy
never builds a protocol string, never parses a reply, never touches a socket. The
strategy classes receive their `GameApi`, channel, recruiter and jammer from the entry
point, so tests replace them with fakes (`tests/test_pack.py`).

## 3. Strategy: the pack climbs together

**Why.** An incantation elevates every same-level player on its tile. The first AI of
the project climbed by pairs (levels 2 to 4 need only 2 players) and the team split into
groups of different levels, for example two players at level 4, two at 5 and two at 6.
Level 5 needs 4 players of the same level, level 6 needs 6: none of these groups could
climb again, and replacements started at level 1 with nobody of their level.

**How** (`logic/pack.py`):

1. Every player reaches level 2 alone: one linemate, solo incantation.
2. The queen stocks 40 food, reaches level 2, then settles on her tile (*home*) and never
   moves again. Every turn she broadcasts an encrypted `Home` message: her level, her
   food, and the stones still missing in her stock for **all** incantations up to level 8.
3. Workers stock 30 food, collect the missing stones and walk home. They follow only the
   **latest** Home sound and forget sounds heard while walking (a direction is relative to
   the position and orientation at the time it was heard). Home never moves, so every
   step brings them one tile closer.
4. At home, workers drop their stones, and food when the queen runs low. The queen picks
   everything up: the stock stays in her inventory, where no other team can take it.
5. When the stock covers the whole climb and 7 players stand at home (6 after a short
   wait), the queen alone drops the stones of the current level and incants. The server
   elevates every same-level player on the tile, so the group climbs **together**, level
   after level. Workers never incant, so incantations never overlap.

Players arriving too late to join the climb keep collecting stones and stay alive.

## 4. Communication: encryption and jamming

Every broadcast is heard by every player of every team, with its direction. A message
in clear text tells the enemy where our home is, and anyone can forge one.

**Protection** (`comm/secure.py`). Each message is
`base64(sender | counter | ciphertext | tag)`:

| Part | How | Protects against |
|---|---|---|
| key | HMAC-SHA256(secret, team name) | other teams: they do not have our key |
| ciphertext | plaintext XOR a SHA-256 keystream (key, sender, counter) | reading: the content is unreadable |
| tag | HMAC-SHA256 of everything, 12 bytes | forging and modifying: the message is rejected |
| counter | increases per sender; older or repeated counters are rejected | replaying a captured message |

The secret defaults to a constant of this code and can be changed per game with
`ZAPPY_TEAM_SECRET`. Limit: an opponent running this very code with the same team name
and secret could derive the key.

**Attack** (`comm/jammer.py`). The queen keeps the last messages she cannot
authenticate (they come from other teams) and, every third turn, broadcasts a corrupted
copy: same shape, digits replaced. A team that talks in clear text receives believable
but false levels, quantities or weights, from the direction of **our** queen. Our own
team rejects these decoys, since they fail authentication.

## 5. Results

Measured with `tools/benchmark.py` (server at `-f 100`, 9 slots per team, map 20×20
unless stated, one laptop). A game stops at the first win (6 players of a team at level 8).

**Strategy** (2026-11-14, 16 games):

| Games | Runs | Result |
| --- | --- | --- |
| first AI vs itself | 5 | **0 wins** after 30,000 time units; max level 5 or 6; 4 to 15 deaths per team |
| pack vs pack | 5 | **5 wins**, after 6,800 to 8,060 time units; 0 or 1 death per team |
| pack vs first AI | 3 | **3 pack wins**, after 6,980 to 8,580; the first AI was at level 4 or 5 |
| 4 pack teams, 10×10 map | 3 | **3 wins**, after 6,730 to 7,260; 0 to 2 deaths per team |

Every win had 7 or 8 players at level 8 together. The first AI is kept in the git tag
`legacy-ai`.

**Communication** (2026-11-14):

Opponent: the same AI with encryption turned off (`ZAPPY_INSECURE=1`), so its messages are
in clear text and unauthenticated. 4 games per row, 20×20 map:

| Games | Our wins | Opponent's max level when it lost |
| --- | --- | --- |
| encrypted, jammer on, vs clear text | 3 / 4 | 6, 4, 5 |
| encrypted, jammer off (control), vs clear text | 3 / 4 | 5, 7, 7 |
| encrypted vs encrypted (sanity check) | 3 / 3 games won by one side | 7, 6, 5 |

What this shows, and what it does not:
- **The first version of the jammer backfired.** A sound always comes from its sender, so
  decoys lured enemy workers onto our home tile. The queen counted players with `Look`,
  which cannot tell teams apart: she started the climb without all her workers and our
  group split (2 games out of 4 stuck at level 5 or 6 after 30,000 time units). Fixed with
  an authenticated roll call: workers waiting at home broadcast an encrypted `Here`
  message with their id, and the queen counts only those. Since then, no game got stuck.
- **The jammer's effect is not significant.** The opponent ends a little lower with the
  jammer on, but 4 games per row cannot separate that from chance. The protection, on the
  other hand, is verified by the unit tests: a foreign, modified or replayed message is
  always rejected.

**Limits.**
- Games stop at the first winner: whether every pack team would also win was not measured.
- This server does not consume food during the 300-unit incantation freeze. With a server
  that does, the climb from level 2 to 8 costs about 15 food per player; workers arrive
  home with 30, but this case was not measured.
- These are a few dozen games: a clear trend, not a proof.

## 6. Running and measuring

```bash
cd ai && python3 zappy_ai -p 4242 -n team1 -h localhost      # one queen; she starts the team
cd ai && python3 -m unittest discover -s tests -t .          # unit tests
python3 tools/benchmark.py --teams red,blue                  # headless game until a team wins
python3 tools/benchmark.py --teams secure,plain:insecure     # vs a team talking in clear text
python3 tools/benchmark.py --teams secure:nojam,plain:insecure   # same, jammer off (control)
```

Environment variables: `ZAPPY_AI_DELAY` (seconds per move, to watch a game),
`ZAPPY_DEBUG=1` (prints all traffic), `ZAPPY_TEAM_SECRET` (team key secret),
`ZAPPY_INSECURE=1` and `ZAPPY_JAM=0` (experiments only: clear text, jammer off).
