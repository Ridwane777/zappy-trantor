"""Encrypted and authenticated team messages (standard library only).

Every broadcast is heard by every player of every team, so a plain "HOME:3:..."
tells the enemy where our home is and lets them forge orders. Each message is:

    base64( sender:4 | counter:8 | ciphertext | tag:12 )

- key        HMAC-SHA256(secret, team name): only our team derives it
- ciphertext plaintext XOR a keystream of SHA-256(key | sender | counter | block)
- tag        HMAC-SHA256(key, sender | counter | ciphertext), truncated to 12 bytes:
             a modified or forged message is rejected
- counter    increases with every message of a sender; a receiver rejects any counter
             it has already seen from that sender, so a captured message cannot be replayed

The secret defaults to a constant of this code base and can be changed for a game
with the ZAPPY_TEAM_SECRET environment variable (inherited by the team's processes).
Limit: an opponent running this very code with the same team name could derive the
key; the secret is what keeps it private.
"""
import base64
import binascii
import hashlib
import hmac
import os
import secrets
import struct

DEFAULT_SECRET = "zappy-trantor/v1"
_HEADER = struct.Struct(">IQ")   # sender id, counter
_TAG_SIZE = 12


class PlainChannel:
    """No protection at all: what a team without encryption does. Only used to
    measure the effect of the jammer (ZAPPY_INSECURE=1), never by default."""

    def seal(self, plaintext: str) -> str:
        return plaintext

    def open(self, token: str):
        return token


def make_channel(team: str):
    if os.getenv("ZAPPY_INSECURE") == "1":
        return PlainChannel()
    return SecureChannel(team)


def team_key(team: str, secret: str = None) -> bytes:
    secret = os.getenv("ZAPPY_TEAM_SECRET", DEFAULT_SECRET) if secret is None else secret
    return hmac.new(secret.encode(), team.encode(), hashlib.sha256).digest()


def _keystream(key: bytes, header: bytes, length: int) -> bytes:
    blocks = []
    for i in range((length + 31) // 32):
        blocks.append(hashlib.sha256(key + header + struct.pack(">I", i)).digest())
    return b"".join(blocks)[:length]


def _xor(data: bytes, stream: bytes) -> bytes:
    return bytes(a ^ b for a, b in zip(data, stream))


class SecureChannel:
    def __init__(self, team: str, secret: str = None, sender_id: int = None):
        self._key = team_key(team, secret)
        self._sender = secrets.randbits(32) if sender_id is None else sender_id
        self._counter = 0
        self._last_seen = {}   # sender id -> highest counter accepted

    def seal(self, plaintext: str) -> str:
        """Encrypts and signs `plaintext`; the result is safe to broadcast."""
        self._counter += 1
        header = _HEADER.pack(self._sender, self._counter)
        data = plaintext.encode()
        body = _xor(data, _keystream(self._key, header, len(data)))
        tag = hmac.new(self._key, header + body, hashlib.sha256).digest()[:_TAG_SIZE]
        return base64.b64encode(header + body + tag).decode()

    def open(self, token: str):
        """The plaintext of a message from our team, or None for anything else:
        another team's message, a forged or modified one, or a replay."""
        try:
            raw = base64.b64decode(token.encode(), validate=True)
        except (binascii.Error, ValueError):
            return None
        if len(raw) < _HEADER.size + _TAG_SIZE:
            return None
        header, body, tag = raw[:_HEADER.size], raw[_HEADER.size:-_TAG_SIZE], raw[-_TAG_SIZE:]
        expected = hmac.new(self._key, header + body, hashlib.sha256).digest()[:_TAG_SIZE]
        if not hmac.compare_digest(tag, expected):
            return None
        sender, counter = _HEADER.unpack(header)
        if sender == self._sender or counter <= self._last_seen.get(sender, 0):
            return None
        self._last_seen[sender] = counter
        try:
            return _xor(body, _keystream(self._key, header, len(body))).decode()
        except UnicodeDecodeError:
            return None
