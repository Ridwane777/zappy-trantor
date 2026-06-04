PREFIX = "ZAPPY_AI"


def encode(msg_type: str, data: str = "") -> str:
    return f"{PREFIX}:{msg_type}:{data}"


def decode(raw: str) -> tuple[int, str, str] | None:
    """
    Parses 'message K, text' from the server.
    Returns (direction, msg_type, data) or None if not ours.
    """
    if not raw.startswith("message "):
        return None
    try:
        header, text = raw[len("message "):].split(", ", 1)
        direction = int(header)
        if not text.startswith(PREFIX + ":"):
            return None
        parts = text[len(PREFIX) + 1:].split(":", 1)
        msg_type = parts[0]
        data = parts[1] if len(parts) > 1 else ""
        return direction, msg_type, data
    except (ValueError, IndexError):
        return None
