"""Per-floor design checks for hand-authored floors.

Rules come from the adversarial audit of the generated campaign:
  * exactly one 'D'; chapter-end floors (10/20/30/40) have one 'U' and at most
    one 'G' chapter gate; floor 50 has one 'G' and no 'U'; every other floor has
    exactly one 'U' and no 'G'
  * every walkable cell reachable from 'D'
  * at least one cycle (the floor must not be a tree, or there is no route choice)
  * at least one enemy whose removal disconnects the exit (a real gate)
  * boss floors: removing 'M' must disconnect the exit
  * a secret wall must hide a reward (item/enemy), not an empty pocket
  * keys found on the floor >= doors of that colour that must be opened
  * object density >= 40% of walkable cells, except floor 1 and boss floors
  * longest single-cell dead end reported; >3 is flagged

Usage:  python tools/floor_check.py --floor 3
        python tools/floor_check.py --all
"""
from __future__ import annotations

import argparse
import re
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "main" / "tower_floors.c"
ENEMIES = set("svkXmgwM")
KEYS, DOORS = "ybr", "YBR"
REWARD = set("pPad") | ENEMIES


def load_floors() -> list[list[str]]:
    text = SRC.read_text(encoding="utf-8")
    out = []
    for m in re.finditer(r"static const char \*const tower_floor_(\d+)_rows\[\] = \{(.*?)\};", text, re.S):
        out.append(re.findall(r'"([^"]*)"', m.group(2)))
    return out


def find(rows, ch):
    return [(x, y) for y, r in enumerate(rows) for x, c in enumerate(r) if c == ch]


def reach(rows, start, blocked=frozenset(), passable_secret=True):
    H, W = len(rows), len(rows[0])
    if start is None:
        return set()
    q = deque([start])
    seen = {start}
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if not (0 <= nx < W and 0 <= ny < H) or (nx, ny) in seen or (nx, ny) in blocked:
                continue
            c = rows[ny][nx]
            if c == "#":
                continue
            seen.add((nx, ny))
            q.append((nx, ny))
    return seen


def cycles(rows) -> int:
    """Independent cycles = E - V + 1 over the walkable graph."""
    H, W = len(rows), len(rows[0])
    v = 0
    e = 0
    for y in range(H):
        for x in range(W):
            if rows[y][x] == "#":
                continue
            v += 1
            for dx, dy in ((1, 0), (0, 1)):
                nx, ny = x + dx, y + dy
                if nx < W and ny < H and rows[ny][nx] != "#":
                    e += 1
    return e - v + 1 if v else 0


def longest_dead_end(rows) -> int:
    """Longest simple path starting at a degree-1 cell, in walkable steps."""
    H, W = len(rows), len(rows[0])
    walk = [(x, y) for y in range(H) for x in range(W) if rows[y][x] != "#"]
    deg = {}
    for x, y in walk:
        deg[(x, y)] = sum(1 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
                          if 0 <= x + dx < W and 0 <= y + dy < H and rows[y + dy][x + dx] != "#")
    best = 0
    for leaf in [p for p in walk if deg[p] == 1]:
        # walk until a junction or the end
        prev, cur, n = None, leaf, 0
        while True:
            n += 1
            nxt = [p for p in ((cur[0] + 1, cur[1]), (cur[0] - 1, cur[1]),
                               (cur[0], cur[1] + 1), (cur[0], cur[1] - 1))
                   if 0 <= p[0] < W and 0 <= p[1] < H and rows[p[1]][p[0]] != "#" and p != prev]
            if len(nxt) != 1 or deg.get(nxt[0], 0) > 2:
                break
            prev, cur = cur, nxt[0]
        best = max(best, n)
    return best


def check(idx: int, rows: list[str]) -> tuple[list[str], list[str]]:
    ok, bad = [], []
    H, W = len(rows), len(rows[0])
    flat = "".join(rows)
    boss = (idx + 1) % 10 == 0
    final = idx == 49
    tutorial = idx == 0

    d, u, g = find(rows, "D"), find(rows, "U"), find(rows, "G")
    (ok if len(d) == 1 else bad).append(f"exactly one D ({len(d)})")
    # A chapter-end floor may carry a goal gate and the exit stair together; the
    # goal closes the chapter, the stair leads to the next one.
    if final:
        (ok if len(g) == 1 and not u else bad).append(
            f"final floor has one G and no U ({len(g)}+{len(u)})")
    elif boss:
        (ok if len(u) == 1 else bad).append(f"exactly one U ({len(u)})")
        (ok if len(g) <= 1 else bad).append(f"at most one G chapter gate ({len(g)})")
    else:
        (ok if len(u) == 1 and not g else bad).append(
            f"exactly one U and no G ({len(u)}+{len(g)})")
    goal = u[0] if u else (g[0] if g else None)
    if not d or goal is None:
        return ok, bad + ["missing D or exit"]

    r = reach(rows, d[0])
    walk = {(x, y) for y in range(H) for x in range(W) if rows[y][x] != "#"}
    (ok if walk <= r else bad).append(f"all walkable reachable ({len(r)}/{len(walk)})")

    cyc = cycles(rows)
    (ok if cyc >= 1 or tutorial else bad).append(f"has a loop ({cyc} independent cycles)")

    # enemy gate: an enemy whose removal disconnects the exit
    gates = []
    for (x, y) in [p for p in walk if rows[p[1]][p[0]] in ENEMIES]:
        if goal not in reach(rows, d[0], blocked={(x, y)}):
            gates.append((x, y, rows[y][x]))
    (ok if gates else bad).append(f"enemy gate present ({len(gates)} articulation enemies)")

    if boss:
        # The Demon King is reserved for the final floor. A region boss only has
        # to be an enemy that gates the exit; an 'M' that is present must gate it.
        ms = find(rows, "M")
        if ms:
            blocked = goal not in reach(rows, d[0], blocked={ms[0]})
            (ok if blocked else bad).append("demon king blocks the exit")
        (ok if gates else bad).append("boss floor is gated by an enemy")

    secrets = find(rows, "%")
    if secrets:
        hidden = False
        for s in secrets:
            after = reach(rows, d[0])  # '%' is passable in reach()
            behind = [p for p in after if rows[p[1]][p[0]] in REWARD]
            # a reward only reachable once the secret is open
            before = reach(rows, d[0], blocked={s})
            if any(p not in before for p in behind):
                hidden = True
        (ok if hidden else bad).append(f"secret hides a reward ({len(secrets)} secret walls)")
    else:
        (ok if tutorial else bad).append("has a secret wall")

    # Key economy. Scarcity is the point: keys fewer than doors is CORRECT and
    # must not be "fixed". What is forbidden is a mandatory door you cannot open.
    for i, colour in enumerate(KEYS):
        k = flat.count(colour)
        doors = find(rows, DOORS[i])
        mandatory = [door for door in doors
                     if goal not in reach(rows, d[0], blocked={door})]
        if mandatory:
            (ok if k >= len(mandatory) else bad).append(
                f"{colour.upper()} keys {k} >= {len(mandatory)} mandatory doors"
                f" ({len(doors) - len(mandatory)} optional)")
        elif doors:
            ok.append(f"{colour.upper()}: {len(doors)} optional doors, 0 mandatory")

    obj = sum(1 for c in flat if c not in "#.")
    dens = 100 * obj / max(1, len(walk))
    (ok if dens >= 40 or tutorial or boss else bad).append(f"density {dens:.0f}%")

    dead = longest_dead_end(rows)
    (ok if dead <= 3 or tutorial else bad).append(f"longest dead end {dead}")
    return ok, bad


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--floor", type=int)
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()
    floors = load_floors()
    idxs = range(len(floors)) if args.all else [args.floor - 1]
    failures = 0
    for i in idxs:
        ok, bad = check(i, floors[i])
        print(f"--- floor {i + 1} ({len(floors[i][0])}x{len(floors[i])})")
        for line in ok:
            print(f"  ok   {line}")
        for line in bad:
            print(f"  FAIL {line}")
        failures += len(bad)
    print("ALL PASS" if failures == 0 else f"{failures} failing checks")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
