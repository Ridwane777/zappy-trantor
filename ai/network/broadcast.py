def encode(msg_type: str, data: str) -> str:
    return f"{msg_type}:{data}"


def decode(raw: str):
    if not raw.startswith("message "):
        return None

    try:
        after = raw[len("message "):]
        k_part, content = after.split(",", 1)
        direction = int(k_part.strip())
        return (direction, content.strip(), "")
    except (ValueError, IndexError):
        return None
