def encode(msg_type: str, data: str) -> str:
    """Build the text of a broadcast message: "TYPE:data".

    Our messages are prefixed with a TYPE (e.g. "NEED") followed by ":" and the data,
    so we can pick out our own messages on reception and ignore the rest.
    Example: encode("NEED", "team1:4:8123") -> "NEED:team1:4:8123".
    """
    return f"{msg_type}:{data}"


def decode(raw: str):
    """Decode a sound received from the server into (direction, type, data).

    The server delivers a sound as:
        "message K, TYPE:data"
    where K is the direction the sound comes from (0 = same tile, 1..8 = tile relative
    to our orientation, see DIRECTION_MOVES in brain.py).

    Returns:
        - a tuple (direction:int, type:str, data:str) if the message is
          valid;
        - None if it is not a sound message or is malformed (it is then
          ignored without crashing).
    """
    if not raw.startswith("message "):
        return None

    try:
        # Strip the "message " prefix, then split direction from content
        # on the FIRST comma only (the content may contain ":").
        after = raw[len("message "):]
        k_part, content = after.split(",", 1)
        direction = int(k_part.strip())

        # The content is "TYPE:data". Without ":", there is no
        # data (message without payload).
        content = content.strip()
        if ":" in content:
            msg_type, data = content.split(":", 1)
        else:
            msg_type, data = content, ""

        return (direction, msg_type.strip(), data.strip())

    except (ValueError, IndexError):
        return None
