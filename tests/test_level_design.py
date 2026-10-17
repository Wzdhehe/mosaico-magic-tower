"""Static design checks for the hand-authored Magic Tower campaign.

Run independently: python tests/test_level_design.py
This is not a substitute for the real C model replay in tests/run_tests.py.
"""
from collections import deque
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
W = H = 11


def load_maps():
    src = (ROOT / "main" / "tower.c").read_text(encoding="utf-8")
    chunk = src.split("initial_map[TOWER_FLOORS]")[1].split("};")[0]
    rows = re.findall(r'"([^"]+)"', chunk)
    assert len(rows) == 10 * H, len(rows)
    assert all(len(row) == W for row in rows)
    return [rows[i * H:(i + 1) * H] for i in range(10)]


def can_reach(m, destination, blocked="", start=(5, 9)):
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
    for i, m in enumerate(maps):
        start = (5, 9)
        assert m[9][5] == "D" if i else m[9][5] == "."
        assert m[1][5] == ("G" if i == 9 else "U")
        # Verify that no open tile is disconnected behind geometry alone.
        assert all(can_reach(m, (x, y))
                   for y in range(H) for x in range(W) if m[y][x] != "#"), i + 1

    # Blue key on 4F before blue gate; another after it unlocks 3F vault.
    assert can_reach(maps[3], (7, 8), "B")
    assert not can_reach(maps[3], (7, 4), "B")
    # A red key cannot be obtained from 3F until its blue vault is unlocked.
    assert not can_reach(maps[2], (1, 4), "B")
    assert not can_reach(maps[3], (2, 5), "R")
    # Major puzzle locks cannot be bypassed by room geometry.
    for floor, target, gate in [
        (4, (5, 1), "R"),  # 5F red archive seal
        (6, (9, 5), "w"),  # 7F wraith guards red key
        (7, (5, 1), "R"),  # 8F red prison seal
        (8, (5, 1), "gw"), # 9F must choose one guard
        (9, (5, 1), "M"),  # 10F boss blocks the finish
    ]:
        assert can_reach(maps[floor], target)
        assert not can_reach(maps[floor], target, gate), (floor + 1, gate)
    assert can_reach(maps[8], (5, 1), "g")
    assert can_reach(maps[8], (5, 1), "w")

    wall_masks = {"".join("#" if c == "#" else "." for row in m for c in row)
                  for m in maps[2:]}
    assert len(wall_masks) >= 7, "Floors 3-10 have too many duplicated layouts"
    print("PASS: ten connected maps, key detours, mandatory and alternative guards")


if __name__ == "__main__":
    main()
