#!/usr/bin/env python3
"""Review hand-authored tower maps for meaningful decisions, not tile quotas.

Uses real C map and monster tables. This is a fixed-stat, single-floor
diagnostic, NOT a complete cross-floor optimal strategy solver.
"""
import argparse
from collections import deque
import heapq
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main" / "tower.c"
SIZE = 11
ENEMY_NAMES = {"s": "SLIME", "v": "BAT", "k": "KNIGHT",
               "X": "SENTINEL", "m": "MAGE", "g": "GOLEM",
               "w": "WRAITH", "M": "DEMON"}
LOOT = set("ypadPbr")


def load_maps(src):
    body = src.split("initial_map[TOWER_FLOORS]", 1)[1].split("};", 1)[0]
    rows = re.findall(r'"([^"\n]+)"', body)
    if not rows or len(rows) % SIZE or any(len(row) != SIZE for row in rows):
        raise ValueError("unexpected initial_map dimensions")
    return [rows[n:n + SIZE] for n in range(0, len(rows), SIZE)]


def load_enemies(src):
    part = src.split("tower_result_t tower_enemy_preview(", 1)[1]
    part = part.split("static void clear_pending(", 1)[0]
    stats = {}
    names = {name: ch for ch, name in ENEMY_NAMES.items()}
    expr = (r"case TOWER_TILE_(\w+):\s*p->hp\s*=\s*(\d+);"
            r"\s*p->attack\s*=\s*(\d+);\s*p->defence\s*=\s*(\d+);"
            r"\s*p->gold\s*=\s*(\d+);")
    for name, hp, atk, defence, gold in re.findall(expr, part):
        if name in names:
            stats[names[name]] = tuple(map(int, (hp, atk, defence, gold)))
    fallback = re.search(
        r"default:\s*p->hp\s*=\s*(\d+);\s*p->attack\s*=\s*(\d+);"
        r"\s*p->defence\s*=\s*(\d+);\s*p->gold\s*=\s*(\d+);", part)
    if fallback and "X" not in stats:
        stats["X"] = tuple(map(int, fallback.groups()))
    if len(stats) != len(ENEMY_NAMES):
        raise ValueError("enemy C switch changed: audit must be updated")
    return stats


def adjacent(x, y):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = x + dx, y + dy
        if 0 <= nx < SIZE and 0 <= ny < SIZE:
            yield nx, ny


def area(grid, blocked="", start=(5, 9)):
    seen = {start}
    todo = deque([start])
    while todo:
        x, y = todo.popleft()
        for nx, ny in adjacent(x, y):
            if (nx, ny) not in seen and grid[ny][nx] not in "#" + blocked:
                seen.add((nx, ny))
                todo.append((nx, ny))
    return seen


def reachable(grid, goal, blocked=""):
    return goal in area(grid, blocked)


def loss(stat, attack, defence):
    hp, enemy_atk, enemy_def, _gold = stat
    if attack <= enemy_def:
        return None
    hits = (hp + attack - enemy_def - 1) // (attack - enemy_def)
    return (hits - 1) * max(0, enemy_atk - defence)


def shortest_guard_path(grid, enemies, attack, defence, blocked=""):
    """Minimize enemy damage, then steps, at a frozen hero-stat snapshot.

    Doors are impassable; item effects do not affect the path search.
    This is only a *candidate path*. Replay pickups in actual order below.
    """
    source, target = (5, 9), (5, 1)
    heap = [(0, 0, source, (source,))]
    best = {source: (0, 0)}
    while heap:
        damage, steps, pos, path = heapq.heappop(heap)
        if (damage, steps) != best.get(pos):
            continue
        if pos == target:
            return {"loss_at_fixed_stats": damage,
                    "steps": steps, "path": path}
        for nx, ny in adjacent(*pos):
            tile = grid[ny][nx]
            if tile in "#YBR" + blocked:
                continue
            cost = loss(enemies[tile], attack, defence) if tile in enemies else 0
            if cost is None:
                continue
            new = (damage + cost, steps + 1)
            if new < best.get((nx, ny), (10**9, 10**9)):
                best[nx, ny] = new
                heapq.heappush(heap, (*new, (nx, ny), path + ((nx, ny),)))
    return None


def replay_candidate(grid, path, enemies, *, hp, attack, defence, gold):
    """Apply pickups before fights; model per-tile consumables on simple path."""
    visited = set()
    fights = []
    for x, y in path[1:]:
        if (x, y) in visited:
            continue
        visited.add((x, y))
        tile = grid[y][x]
        if tile == "a":
            attack += 5
        elif tile == "d":
            defence += 3
        elif tile == "p":
            hp += 30
        elif tile == "P":
            hp += 80
        elif tile in enemies:
            dmg = loss(enemies[tile], attack, defence)
            if dmg is None or dmg >= hp:
                return None
            hp -= dmg
            gold += enemies[tile][3]
            fights.append((tile, dmg))
    return dict(hp=hp, attack=attack, defence=defence,
                gold=gold, fights=fights)


def rewards_without(grid, blocked):
    reachable_cells = area(grid, blocked + "YBR")
    return {(grid[y][x], x, y) for x, y in reachable_cells
            if grid[y][x] in LOOT}


def audit(src):
    maps, enemies = load_maps(src), load_enemies(src)
    floor9 = maps[8]
    g = shortest_guard_path(floor9, enemies, 62, 28, blocked="w")
    w = shortest_guard_path(floor9, enemies, 62, 28, blocked="g")
    replay = dict(hp=701, attack=62, defence=28, gold=169)
    stats = {
        "floors": len(maps),
        "wall_pct": round(100 * sum(row.count("#") for floor in maps for row in floor)
                          / (len(maps) * SIZE * SIZE), 1),
        "distinct_walls_f3_plus": len({
            "".join("#" if c == "#" else "." for row in floor for c in row)
            for floor in maps[2:]}),
        "objects_per_walkable_pct": round(
            100 * sum(c not in "#." for f in maps for r in f for c in r)
            / sum(c != "#" for f in maps for r in f for c in r), 1),
        "f4_quiet": not any(c in enemies for r in maps[3] for c in r),
        "f5_red_seal": reachable(maps[4], (5, 1)) and
                       not reachable(maps[4], (5, 1), "R"),
        "f7_wraith_key": reachable(maps[6], (9, 5)) and
                         not reachable(maps[6], (9, 5), "w"),
        "f8_red_seal": reachable(maps[7], (5, 1)) and
                       not reachable(maps[7], (5, 1), "R"),
        "f9_golem_path": g, "f9_wraith_path": w,
        "f9_golem_replay": replay_candidate(floor9, g["path"], enemies, **replay)
                             if g else None,
        "f9_wraith_replay": replay_candidate(floor9, w["path"], enemies, **replay)
                              if w else None,
        "f9_same_accessible_loot": rewards_without(floor9, "w")
                                   == rewards_without(floor9, "g"),
        "f9_both_blocked": not reachable(floor9, (5, 1), "gw"),
        "f10_boss_required": reachable(maps[9], (5, 1)) and
                             not reachable(maps[9], (5, 1), "M"),
        "sentinel_gold_inconsistency":
            enemies["X"][3] == 0 and
            "case TOWER_TILE_SENTINEL: return 60;" in src,
    }
    return stats


def contract(stats):
    """Only structural invariants get CI failures; balance stays advisory."""
    return {
        "5F/8F red locks gate their exits":
            stats["f5_red_seal"] and stats["f8_red_seal"],
        "7F wraith gates its red key": stats["f7_wraith_key"],
        "9F must defeat one of two guards":
            stats["f9_golem_path"] is not None and
            stats["f9_wraith_path"] is not None and
            stats["f9_both_blocked"],
        "10F boss is unavoidable": stats["f10_boss_required"],
    }


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument("--check", action="store_true")
    args = cli.parse_args()
    result = audit(SOURCE.read_text(encoding="utf-8"))
    print(f"{result['floors']} floors; walls {result['wall_pct']}%; "
          f"occupied walkable cells {result['objects_per_walkable_pct']}%; "
          f"unique 3F+ wall masks {result['distinct_walls_f3_plus']}")
    for prefix in ("golem", "wraith"):
        path, outcome = result["f9_" + prefix + "_path"], result["f9_" + prefix + "_replay"]
        print(f"9F {prefix}: frozen HP cost={path['loss_at_fixed_stats']}, "
              f"steps={path['steps']}, actual ordered pickup result={outcome}")
    print("9F both routes expose same loot:", result["f9_same_accessible_loot"])
    print("4F deliberately has no combat:", result["f4_quiet"])
    print("Warning: sentinel preview/save-gold mismatch:",
          result["sentinel_gold_inconsistency"])
    checks = contract(result)
    for name, ok in checks.items():
        print(("PASS" if ok else "FAIL") + ": " + name)
    if args.check and not all(checks.values()):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
