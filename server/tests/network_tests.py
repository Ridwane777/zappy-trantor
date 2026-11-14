#!/usr/bin/env python3
"""Network robustness tests for zappy_server, the same checks as the evaluation grid.

Starts its own server (default ./zappy_server next to this folder), then:
  1. fragmentation : "tea", pause, "m1\\n" must be read as one team name
  2. flood         : a client sends "team1" lines non-stop (like `while true; do echo team1; done | nc`)
                     while another client must still be served quickly
  3. urandom       : a client sends random bytes; the server stays up and serves others
  4. no reader     : a client floods commands and never reads its replies; it gets dropped,
                     the server memory stays bounded
  5. load          : 300 clients connect at once and all get WELCOME
Exit code 0 when every test passes.
"""
import os
import socket
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = os.environ.get("ZAPPY_SERVER", os.path.join(HERE, "..", "zappy_server"))
PORT = int(os.environ.get("ZAPPY_TEST_PORT", "4790"))


def connect(timeout=3.0):
    s = socket.create_connection(("127.0.0.1", PORT), timeout=timeout)
    s.settimeout(timeout)
    return s


def read_line(s):
    buf = b""
    while not buf.endswith(b"\n"):
        chunk = s.recv(1)
        if not chunk:
            raise ConnectionError("closed")
        buf += chunk
    return buf.decode().strip()


def handshake(team="team1"):
    s = connect()
    assert read_line(s) == "WELCOME"
    s.sendall(team.encode() + b"\n")
    slots = read_line(s)
    dims = read_line(s)
    return s, slots, dims


def server_rss_kb(proc):
    with open(f"/proc/{proc.pid}/status") as f:
        for line in f:
            if line.startswith("VmRSS:"):
                return int(line.split()[1])
    return 0


def test_fragmentation(_):
    s = connect()
    assert read_line(s) == "WELCOME"
    s.sendall(b"tea")
    time.sleep(0.5)
    s.sendall(b"m1\n")
    slots, dims = read_line(s), read_line(s)
    assert slots.isdigit() and dims == "10 10", (slots, dims)
    s.close()


def test_flood(proc):
    stop = threading.Event()

    def flood():
        s = connect()
        s.settimeout(0.2)
        while not stop.is_set():
            try:
                s.sendall(b"team1\n" * 200)
                s.recv(65536)
            except (socket.timeout, BlockingIOError):
                pass
            except OSError:
                return
        s.close()

    t = threading.Thread(target=flood)
    t.start()
    time.sleep(1.0)
    start = time.time()
    s, _, _ = handshake("team2")
    s.sendall(b"Inventory\n")
    reply = read_line(s)
    latency = time.time() - start
    stop.set()
    t.join()
    assert reply.startswith("[food"), reply
    assert latency < 1.0, f"other client served in {latency:.2f}s during the flood"
    assert proc.poll() is None, "server died"
    s.close()


def test_urandom(proc):
    s = connect()
    end = time.time() + 2
    try:
        while time.time() < end:
            s.sendall(os.urandom(4096))
    except OSError:
        pass  # the server may drop us for an over-long line: fine
    s.close()
    assert proc.poll() is None, "server died"
    other, _, _ = handshake("team2")
    other.close()


def test_no_reader(proc):
    """The evaluation's flood, worst case: `nc` prints replies slowly, so here the
    client sends unknown lines (each one answered) for 5 s and never reads."""
    rss_before = server_rss_kb(proc)
    s, _, _ = handshake("team1")
    end = time.time() + 5
    try:
        while time.time() < end:
            s.sendall(b"team1\n" * 500)
    except OSError:
        pass  # dropped by the server: expected
    start = time.time()
    other, _, _ = handshake("team2")
    other.sendall(b"Inventory\n")
    read_line(other)
    latency = time.time() - start
    assert proc.poll() is None, "server died"
    growth = server_rss_kb(proc) - rss_before
    assert growth < 50_000, f"server memory grew by {growth} kB"
    assert latency < 1.0, f"other client served in {latency:.2f}s"
    s.close()
    other.close()


def test_load(proc):
    socks = []
    for _ in range(300):
        socks.append(connect())
    for s in socks:
        assert read_line(s) == "WELCOME"
        s.close()
    assert proc.poll() is None, "server died"


def main():
    proc = subprocess.Popen([SERVER, "-p", str(PORT), "-x", "10", "-y", "10", "-n", "team1", "team2",
                             "-c", "200", "-f", "100"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.4)
    failures = 0
    tests = (test_fragmentation, test_flood, test_urandom, test_no_reader, test_load)
    try:
        for test in tests:
            try:
                test(proc)
                print(f"[OK]   {test.__name__}")
            except Exception as e:  # report every test, do not stop at the first failure
                failures += 1
                print(f"[FAIL] {test.__name__}: {e!r}")
    finally:
        proc.kill()
    print(f"{len(tests) - failures}/{len(tests)} network tests passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
