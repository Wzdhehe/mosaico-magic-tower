"""Static design checks for the hand-authored Magic Tower campaign.

Run independently: python tests/test_level_design.py
This is not a substitute for the real C model replay in tests/run_tests.py.
"""
from collections import Counter, deque
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
W = H = 11
FLOORS = 10


def load_maps():
    src = (ROOT / "main" / "tower.c").read_text(encoding="utf-8")
    chunk = src.split("initial_map[TOWER_FLOORS]")[1].split("};")[0]
    rows = re.findall(r'"([^"]+)"', chunk)
    assert len(rows) == FLOORS * H, len(rows)
    assert all(len(row) == W for row in rows)
    return [rows[i * H:(i + 1) * H] for i in range(FLOORS)]


def find(m, char):
    return next(((x, y) for y, row in enumerate(m) for x, c in enumerate(row)
                 if c == char), None)


def all_find(m, char):
    return {(x, y) for y, row in enumerate(m) for x, c in enumerate(row)
            if c == char}


def entry(m, index):
    """The player enters on the down stair, except at the ground floor."""
    if index == 0:
        return (5, 9)
    down = find(m, "D")
    assert down is not None, f"floor {index + 1} has no down stair"
    return down


def can_reach(m, destination, blocked="", start=None):
    start = start or entry(m, 0)
    queue = deque([start])
    seen = {start}
    while queue:
        x, y = queue.popleft()
        if (x, y) == destination:
            return True
        for nx, ny in ((x + 1, y), (x - 1, y),
                       (x, y + 1), (x, y - 1)):
            if (0 <= nx < W and 0 <= ny < H and
                    m[ny][nx] not in "#" + blocked and
                    (nx, ny) not in seen):
                seen.add((nx, ny))
                queue.append((nx, ny))
    return False


def main():
    maps = load_maps()

    def reach(index, destination, blocked=""):
        m = maps[index]
        return can_reach(m, destination, blocked, start=entry(m, index))

    # Geometry, stairs and scene plans.
    pairs = set()
    for index, m in enumerate(maps):
        start = entry(m, index)
        assert m[start[1]][start[0]] != "#", (index + 1, start)
        assert all(can_reach(m, (x, y), start=start)
                   for y in range(H) for x in range(W) if m[y][x] != "#"), index + 1
        up, down = find(m, "U"), find(m, "D")
        if index == 0:
            assert up is not None and down is None, "1F is the ground floor"
        elif index == FLOORS - 1:
            assert up is None and down is not None, "10F is the summit"
        else:
            assert up is not None and down is not None, index + 1
            assert up != down, f"floor {index + 1} stairs share a cell"
            pairs.add((up, down))
        if up and down:
            assert up != down
    assert len(pairs) >= 8, f"only {len(pairs)} distinct stair arrangements"

    wall_masks = {"".join("#" if c == "#" else "." for row in m for c in row)
                  for m in maps}
    assert len(wall_masks) == FLOORS, "floor geometry is reused"
    for index, m in enumerate(maps):
        walkable = sum(c != "#" for row in m for c in row)
        walls = sum(c == "#" for row in m for c in row)
        objects = sum(c not in "#." for row in m for c in row)
        assert 0.40 <= walls / 121 <= 0.60, (index + 1, walls)
        assert objects / walkable >= 0.20, (index + 1, objects, walkable)

    # Key and door budget: no colour is drowned in surplus keys.
    keys = Counter(c for m in maps for row in m for c in row if c in "ybr")
    doors = Counter(c for m in maps for row in m for c in row if c in "YBR")
    total_keys, total_doors = sum(keys.values()), sum(doors.values())
    assert 0.90 <= total_keys / total_doors <= 1.15, (total_keys, total_doors)
    for colour in "ybr":
        assert keys[colour] == doors[colour.upper()], (colour, keys[colour], doors[colour.upper()])

    # 4F carries the two blue keys: one opens its own vault, the second
    # unlocks 3F's red vault. Neither is reachable around its blue door.
    assert reach(3, (3, 6))
    assert reach(3, (8, 8))
    assert not reach(3, (8, 8), "B")
    assert reach(2, (3, 3))
    assert not reach(2, (3, 3), "B")

    # 6F: one yellow key sits between the mage and golem. Either guard can
    # be fought, but the key cannot be walked around both.
    assert reach(5, (5, 8), "m")
    assert reach(5, (5, 8), "g")
    assert not reach(5, (5, 8), "mg")

    # Required late-game gates.
    assert reach(4, (1, 8))
    assert not reach(4, (1, 8), "R")          # 5F red seal
    assert reach(6, (8, 8))
    assert not reach(6, (8, 8), "w")          # 7F wraith red key
    assert reach(7, (9, 2))
    assert not reach(7, (9, 2), "R")          # 8F red seal
    assert reach(8, (1, 9))
    assert not reach(8, (1, 9), "w")          # 9F exit wraith
    assert reach(9, (5, 1))
    assert not reach(9, (5, 1), "M")          # 10F demon aisle

    print("PASS: ten distinct scenes, asymmetric stairs, key detours and guarded exits")


if __name__ == "__main__":
    main()
