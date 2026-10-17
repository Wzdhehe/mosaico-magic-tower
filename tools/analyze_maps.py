#!/usr/bin/env python3
"""Analyse the tower maps for design defects.

The map data is read straight out of ``main/tower.c`` so the report always
describes the shipped game rather than a copy of it.  The checks follow
``docs/research/magic-tower-map-design-skill.md``.

    python tools/analyze_maps.py            # full report
    python tools/analyze_maps.py --check    # exit 1 when a check fails
"""
import argparse
import re
import sys
from collections import Counter, deque
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / "main" / "tower.c"

W = H = 11
FLOORS = 10

ENEMY = set("svkXmgwM")
WALL = "#"

NAMES = {
    "y": "yellow key", "b": "blue key", "r": "red key",
    "Y": "yellow door", "B": "blue door", "R": "red door",
    "p": "potion", "P": "big potion", "a": "attack gem", "d": "defence gem",
    "s": "slime", "v": "bat", "k": "knight", "X": "sentinel", "m": "mage",
    "g": "golem", "w": "wraith", "M": "demon", "S": "shop", "n": "npc",
    "U": "up stair", "D": "down stair", "G": "goal", ".": "floor", "#": "wall",
}

# Enemy stats, mirrored from main/tower.c: tower_enemy_preview().
ENEMY_STATS = {
    "s": (24, 6, 2, 3), "v": (35, 11, 4, 6), "k": (70, 20, 8, 12),
    "X": (999, 200, 120, 0), "m": (120, 22, 10, 18), "g": (180, 30, 16, 26),
    "w": (260, 36, 22, 34), "M": (700, 58, 46, 150),
}

# Reference budget from docs/research/magic-tower-map-design-skill.md.
TARGET_WALL = (42.0, 52.0)
TARGET_EMPTY = (12.0, 22.0)
TARGET_ENEMIES_PER_FLOOR = (8, 20)
TARGET_TOTAL_KD = (0.90, 1.15)


def load_maps():
    """Pull the initial_map table out of main/tower.c."""
    text = SOURCE.read_text(encoding="utf-8")
    if "initial_map[TOWER_FLOORS]" not in text:
        raise SystemExit(f"initial_map not found in {SOURCE}")
    block = text.split("initial_map[TOWER_FLOORS]")[1].split("};")[0]
    rows = re.findall(r'"([^"]*)"', block)
    if len(rows) != FLOORS * H:
        raise SystemExit(f"expected {FLOORS * H} map rows, found {len(rows)}")
    if any(len(r) != W for r in rows):
        raise SystemExit("map rows are not all the same width")
    return [rows[f * H:(f + 1) * H] for f in range(FLOORS)]


def neighbours(x, y):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = x + dx, y + dy
        if 0 <= nx < W and 0 <= ny < H:
            yield nx, ny


def reachable(grid, start):
    """Cells reachable from start, treating doors as passable."""
    seen = {start}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        for nx, ny in neighbours(x, y):
            if (nx, ny) in seen or grid[ny][nx] == WALL:
                continue
            seen.add((nx, ny))
            queue.append((nx, ny))
    return seen


def mirror_symmetric(grid):
    return all(grid[y][x] == grid[y][W - 1 - x]
               for y in range(H) for x in range(W))


def analyse(maps, verbose=True):
    results = []      # (name, ok, detail)
    stair_pairs_all = []   # (up, down) per floor, including one-sided floors
    total_walkable = 0
    total_cells = FLOORS * W * H
    stair_pairs = []
    per_floor_enemies = []

    def say(text=""):
        if verbose:
            print(text)

    say("=" * 78)
    say("TOWER MAP ANALYSIS  (11x11 x 10 floors)")
    say("=" * 78)

    for f, grid in enumerate(maps):
        c = Counter("".join(grid))
        n_walk = sum(v for k, v in c.items() if k != WALL)
        total_walkable += n_walk
        per_floor_enemies.append(sum(c[ch] for ch in ENEMY))

        up = [(x, y) for y in range(H) for x in range(W) if grid[y][x] == "U"]
        dn = [(x, y) for y in range(H) for x in range(W) if grid[y][x] == "D"]
        goal = [(x, y) for y in range(H) for x in range(W) if grid[y][x] == "G"]
        start = up[0] if up else (5, 9)
        seen = reachable(grid, start)
        unreachable = [(x, y) for y in range(H) for x in range(W)
                       if grid[y][x] != WALL and (x, y) not in seen]

        dead = [(x, y) for y in range(H) for x in range(W)
                if grid[y][x] != WALL
                and sum(1 for nx, ny in neighbours(x, y)
                        if grid[ny][nx] != WALL) == 1]

        if up and dn:
            stair_pairs.append((up[0], dn[0]))
        if up or dn:
            stair_pairs_all.append((up[0] if up else None, dn[0] if dn else None))

        say(f"\n--- Floor {f + 1:2d} ---")
        say(f"  walkable : {n_walk:3d}/{W * H} ({100 * n_walk / (W * H):3.0f}%)"
            f"   walls {100 * c[WALL] / (W * H):3.0f}%"
            f"   empty {100 * c['.'] / (W * H):4.1f}%"
            f"   objects {100 * (n_walk - c['.']) / (W * H):4.1f}%")
        say(f"  stairs   : up {up[0] if up else '-'}  down {dn[0] if dn else '-'}"
            f"  goal {goal[0] if goal else '-'}")
        say(f"  enemies  : {per_floor_enemies[-1]}"
            + (f"   ({', '.join(f'{NAMES[ch]}x{c[ch]}' for ch in ENEMY if c[ch])})"
               if per_floor_enemies[-1] else "   (none)"))
        say(f"  items    : " + (", ".join(
            f"{NAMES[ch]}x{c[ch]}" for ch in
            "yb rYBRpPadSnUG" .replace(" ", "") if c[ch]) or "none"))
        say(f"  dead ends: {len(dead)}"
            + ("   " + ", ".join(f"{NAMES[grid[y][x]]}@({x},{y})"
                                 for x, y in dead) if dead else ""))
        if unreachable:
            say(f"  !! UNREACHABLE from the up stair: "
                + ", ".join(f"{NAMES[grid[y][x]]}@({x},{y})"
                            for x, y in unreachable))
        if mirror_symmetric(grid):
            say("  symmetry : mirror-symmetric on the vertical axis")

    # ---- cross-floor statistics ----
    glob = Counter("".join("".join(g) for g in maps))
    keys = {col: glob[col] for col in "ybr"}
    doors = {col: glob[col.upper()] for col in "ybr"}
    total_keys = sum(keys.values())
    total_doors = sum(doors.values())
    kd = total_keys / total_doors if total_doors else 0.0
    wall_pct = 100.0 * (total_cells - total_walkable) / total_cells
    empty_pct = 100.0 * glob["."] / total_cells
    obj_pct = 100.0 * (total_walkable - glob["."]) / total_cells
    asym = sum(1 for u, d in stair_pairs if u != d)
    avg_enemies = sum(per_floor_enemies) / FLOORS

    say("\n" + "=" * 78)
    say("CROSS-FLOOR STATISTICS")
    say("=" * 78)
    say(f"  wall {wall_pct:.1f}%   empty {empty_pct:.1f}%   objects {obj_pct:.1f}%"
        f"   (objects among walkable cells: {100.0 * obj_pct / (100.0 - wall_pct):.1f}%)")
    say(f"  keys {total_keys} / doors {total_doors}  ->  K/D = {kd:.3f}")
    for col in "ybr":
        say(f"    {col}: {keys[col]:2d} keys vs {doors[col]:2d} doors"
            f"  -> {keys[col] / doors[col] if doors[col] else float('inf'):.2f}")
    say(f"  stairs on different cells : {asym}/{len(stair_pairs)}")
    say(f"  enemies per floor         : {per_floor_enemies}  (avg {avg_enemies:.1f})")
    say(f"  HP items {glob['p']} small + {glob['P']} large"
        f" = {30 * glob['p'] + 80 * glob['P']} HP")
    say(f"  attack gems {glob['a']}   defence gems {glob['d']}")

    # ---- player stat curve ----
    say("\n  player stat curve (collecting everything, no shopping):")
    hp, atk, dfn = 100, 12, 4
    for f, grid in enumerate(maps):
        c = Counter("".join(grid))
        hp += 30 * c["p"] + 80 * c["P"]
        atk += 5 * c["a"]
        dfn += 3 * c["d"]
        say(f"    after floor {f + 1:2d}: HP={hp:4d} ATK={atk:3d} DEF={dfn:3d}")

    # ---- checks ----
    say("\n" + "=" * 78)
    say("CHECKS (targets from docs/research/magic-tower-map-design-skill.md)")
    say("=" * 78)

    def check(name, ok, detail):
        results.append((name, ok, detail))
        say(f"  [{'PASS' if ok else 'FAIL'}] {name:44s} {detail}")

    check("stairs on different cells (target 100%)",
          asym == len(stair_pairs) and stair_pairs,
          f"{asym}/{len(stair_pairs)}")
    distinct_pairs = len({(u, d) for u, d in stair_pairs_all if u and d})
    check("stair layout varies between floors (target >= 3)",
          distinct_pairs >= 3,
          f"{distinct_pairs} distinct (up, down) pairs")
    check(f"wall share {TARGET_WALL}",
          TARGET_WALL[0] <= wall_pct <= TARGET_WALL[1],
          f"{wall_pct:.1f}%")
    check(f"empty share {TARGET_EMPTY}",
          TARGET_EMPTY[0] <= empty_pct <= TARGET_EMPTY[1],
          f"{empty_pct:.1f}%")
    check(f"enemies per floor {TARGET_ENEMIES_PER_FLOOR}",
          all(TARGET_ENEMIES_PER_FLOOR[0] <= n <= TARGET_ENEMIES_PER_FLOOR[1]
              for n in per_floor_enemies),
          f"min {min(per_floor_enemies)} max {max(per_floor_enemies)}")
    obj_density = 100.0 * obj_pct / (100.0 - wall_pct)
    check("object density among walkable cells (target >= 50%)",
          obj_density >= 50.0,
          f"{obj_density:.1f}%")
    check(f"total K/D {TARGET_TOTAL_KD}",
          TARGET_TOTAL_KD[0] <= kd <= TARGET_TOTAL_KD[1],
          f"{kd:.3f}")
    check("red keys < red doors", keys["r"] < doors["r"],
          f"{keys['r']} vs {doors['r']}")
    check("no floor without enemies",
          all(n > 0 for n in per_floor_enemies),
          f"floors with 0: {[i + 1 for i, n in enumerate(per_floor_enemies) if n == 0]}")

    failed = [r for r in results if not r[1]]
    say(f"\n  {len(results) - len(failed)}/{len(results)} checks passed")
    if failed:
        say("  failed: " + ", ".join(r[0] for r in failed))
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="exit non-zero when a check fails")
    ap.add_argument("--quiet", action="store_true", help="only print the checks")
    args = ap.parse_args()

    maps = load_maps()
    results = analyse(maps, verbose=not args.quiet)
    if args.check and any(not ok for _, ok, _ in results):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
