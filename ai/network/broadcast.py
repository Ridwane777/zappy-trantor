def encode(msg_type: str, data: str) -> str:
    return f"{msg_type}:{data}"


def decode(raw: str):
    if not raw.startswith("message "):
        return None

    try:
        after = raw[len("message "):]
        k_part, content = after.split(",", 1)
        direction = int(k_part.strip())

        content = content.strip()
        if ":" in content:
            msg_type, data = content.split(":", 1)
        else:
            msg_type, data = content, ""

        return (direction, msg_type.strip(), data.strip())

    except (ValueError, IndexError):
        return None