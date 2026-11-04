import re
from pathlib import Path


MAX_ELEMENTS = 256
MAX_UINT32 = 4_294_967_295


def load_values(path: Path) -> tuple[int, ...]:
    tokens: list[str] = []
    for raw_line in path.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if line:
            tokens.extend(token for token in re.split(r"[\s,]+", line) if token)

    if not tokens:
        raise ValueError("input must contain at least one value")
    if len(tokens) > MAX_ELEMENTS:
        raise ValueError(f"input supports at most {MAX_ELEMENTS} values")

    values: list[int] = []
    for token in tokens:
        try:
            value = int(token, 10)
        except ValueError as error:
            raise ValueError(f"invalid integer in input: {token!r}") from error
        if value < 0 or value > MAX_UINT32:
            raise ValueError(f"input value is outside uint32: {value}")
        values.append(value)

    if sum(values) > MAX_UINT32:
        raise ValueError("array sum must fit in uint32")
    return tuple(values)
