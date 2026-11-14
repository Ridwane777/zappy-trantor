#!/usr/bin/env python3
"""Headless benchmark: run a server, one AI per team and a GUI-protocol observer.

Reports, at regular intervals, each team's alive players, births, deaths and level
distribution, and stops when a team wins (6 players at level 8) or time runs out.

Examples (from the repository root, after `make server`):
    python3 tools/benchmark.py --teams red,blue
    python3 tools/benchmark.py --teams new:pack,old:legacy --seconds 300
    python3 tools/benchmark.py --teams a,b,c,d --size 10 --port 4300

A team is NAME or NAME:STRATEGY (pack = default, legacy = previous AI).
"""
import argparse
import collections
import os
import signal
import socket
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--teams", default="team1,team2", help="comma-separated NAME[:STRATEGY]")
    ap.add_argument("--port", type=int, default=4300)
    ap.add_argument("--freq", type=int, default=100, help="server frequency (time units per second)")
    ap.add_argument("--size", type=int, default=20, help="map width and height")
    ap.add_argument("--slots", type=int, default=9, help="initial slots per team (-c)")
    ap.add_argument("--seconds", type=float, default=300, help="wall-clock limit")
    args = ap.parse_args()

    specs = [t.split(":") for t in args.teams.split(",")]
    teams = [s[0] for s in specs]
    server = subprocess.Popen(
        [os.path.join(ROOT, "server", "zappy_server"), "-p", str(args.port),
         "-x", str(args.size), "-y", str(args.size), "-n", *teams,
         "-c", str(args.slots), "-f", str(args.freq)],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.5)
    ais = [subprocess.Popen(
        [sys.executable, "zappy_ai", "-p", str(args.port), "-n", s[0]], cwd=os.path.join(ROOT, "ai"),
        env=dict(os.environ, ZAPPY_STRATEGY=s[1] if len(s) > 1 else "pack"),
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        for s in specs]

    gui = socket.create_connection(("localhost", args.port))
    gui.settimeout(0.5)
    gui.sendall(b"GRAPHIC\n")

    level, team_of, alive = {}, {}, set()
    born, deaths = collections.Counter(), collections.Counter()
    buf, start, last, winner = b"", time.time(), 0.0, None

    def report(tag=""):
        elapsed = time.time() - start
        for t in teams:
            lv = collections.Counter(level[p] for p in alive if team_of.get(p) == t)
            print(f"{elapsed:6.0f}s ~{elapsed * args.freq:7.0f} units  {t}: alive={sum(lv.values())} "
                  f"born={born[t]} dead={deaths[t]} levels={dict(sorted(lv.items()))} "
                  f"max={max(lv) if lv else 0} {tag}", flush=True)

    try:
        while time.time() - start < args.seconds and winner is None:
            try:
                data = gui.recv(65536)
                if not data:
                    break
                buf += data
            except socket.timeout:
                pass
            *lines, buf = buf.split(b"\n")
            for line in lines:
                w = line.decode(errors="replace").split()
                if not w:
                    continue
                if w[0] == "pnw":      # pnw #id x y orientation level team
                    level[w[1]], team_of[w[1]] = int(w[5]), w[6]
                    alive.add(w[1])
                    born[w[6]] += 1
                elif w[0] == "plv":    # plv #id level
                    level[w[1]] = int(w[2])
                elif w[0] == "pdi" and w[1] in alive:
                    alive.discard(w[1])
                    deaths[team_of.get(w[1])] += 1
                elif w[0] == "seg":    # end of game: team w[1] won
                    winner = w[1]
            if time.time() - last > args.seconds / 10:
                last = time.time()
                report()
        report(f"WINNER={winner}")
    finally:
        for ai in ais:
            try:
                os.killpg(ai.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        server.kill()


if __name__ == "__main__":
    main()
