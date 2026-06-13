def encode(msg_type: str, data: str) -> str:
    return f"{msg_type}:{data}"


def decode(raw: str):
    pass