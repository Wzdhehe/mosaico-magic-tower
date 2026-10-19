"""Print a short hand-editing design sheet for one floor.

    python tools/floor_report.py --floor 4

The sheet is derived entirely from ``main/tower_floors.c`` (plus the shared
solver), so it can be re-run after every hand edit.  It reports:

  * the floor's region-I role (draft assignment from the current scaffold),
  * key flow: entry carry -> gained -> spent (mandatory doors) -> carried,
  * enemies on articulation cells (all routes to the exit or that branch
    must cross them), including what they gate,
  * avoidable enemies (not on any articulation cell; deletion candidates),
  * the secret wall position and what it actually rewards,
  * single-cell dead-end corridors longer than three cells.
"""
from __future__ import annotations

import argparse
import sys
from collections import Counter, deque
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import gen_tower  # noqa: E402
import solve_tower  # noqa: E402

NEIGHBOURS = ((1, 0), (-1, 0), (0, 1), (0, -1))
REGION_LABELS = "I II III IV V".split()

# Scaffold's ten-floor beat.  The current draft follows the generator plan:
# production floors are the key/potion-heavy defaults, shop is 6F, supply is
# 9F and the sparse demon floor is 10F.  1F is the teaching floor and the
# branch-heavy floors are the hubs; the two knight-gated floors are pressure.
ROLES = {
    0: "teaching",
    1: "production",
    2: "hub",
    3: "production",
    4: "pressure",
    5: "shop",
    6: "pressure",
    7: "hub",
    8: "supply",
    9: "boss",
}

PICKUPS = frozenset("ybrpPad")
KEY_ORDER = "ybr"
TOTAL_KEYS = 3


def cls(index: int) -> dict:
    """Read one floor from the checked-in C table."""
    floors = gen_tower.read_generated_c(PROJECT_ROOT / "main" / "tower_floors.c")
    if not 0 <= index < len(floors):
        raise SystemExit(f"floor {index + 1} out of range 1..{len(floors)}")
    return {
        "index": index,
        "rows": floors[index],
        "floors": floors,
        "size": len(floors[index]),
        "region": index // gen_tower.REGION,
        "role": ROLES[index % gen_tower.REGION],
    }


def find_char(rows: list[str], char: str):
    for y, row in enumerate(rows):
        x = row.find(char)
        if x >= 0:
            return (x, y)
    return None


def nodes_of(rows: list[str]) -> set[tuple[int, int]]:
    return {(x, y) for y, row in enumerate(rows)
            for x, ch in enumerate(row) if ch != "#"}


def neighbours(cell, nodes):
    x, y = cell
    return [(x + dx, y + dy) for dx, dy in NEIGHBOURS
            if (x + dx, y + dy) in nodes]


def reach(nodes, start, blocked=frozenset()):
    seen = {start}
    queue = deque([start])
    while queue:
        cell = queue.popleft()
        for nxt in neighbours(cell, nodes):
            if nxt in seen or nxt in blocked:
                continue
            seen.add(nxt)
            queue.append(nxt)
    return seen


def articulation_points(nodes) -> set[tuple[int, int]]:
    """Cut vertices of the non-wall graph (all doors/items passable)."""
    ordered = sorted(nodes)
    if len(ordered) <= 2:
        return set(ordered)
    points = set()
    for removed in ordered:
        start = next(cell for cell in ordered if cell != removed)
        seen = reach(nodes, start, {removed})
        if len(seen) != len(ordered) - 1:
            points.add(removed)
    return points


def dead_end_paths(nodes):
    """Paths from a degree-1 cell until the first non-corridor cell."""
    degree = {cell: len(neighbours(cell, nodes)) for cell in nodes}
    paths = []
    for start in sorted(nodes):
        if degree[start] != 1:
            continue
        path = [start]
        previous = None
        current = start
        while True:
            options = [cell for cell in neighbours(current, nodes)
                       if cell != previous]
            if len(options) != 1:
                break
            nxt = options[0]
            path.append(nxt)
            previous, current = current, nxt
            if degree[current] != 2 or len(path) > 400:
                break
        paths.append(path)
    return paths


def mandatory_doors(nodes, start, goal, rows):
    out = []
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in gen_tower.DOOR:
                if goal not in reach(nodes, start, {(x, y)}):
                    out.append((x, y, ch))
    return out


def key_flow(model):
    """entry -> collected -> spent -> carried for the target floor."""
    floors = model["floors"]
    target = model["index"]
    hero = solve_tower.Hero()
    entries = []
    results = []
    failed_at = None
    for index in range(target + 1):
        entries.append(hero.clone())
        result = solve_tower.analyze_floor(floors[index], index, hero)
        results.append(result)
        if not result.solvable:
            failed_at = index
            # Pad so the requested floor always has an entry state to show,
            # even when an earlier hand-edited floor broke the chain.
            while len(entries) <= target:
                entries.append(hero.clone())
                results.append(None)
            break
        hero = result.hero

    entry = entries[target]
    result = results[target] if target < len(results) else None
    placed = Counter(ch for row in model["rows"] for ch in row
                     if ch in gen_tower.KEY)
    placed_vec = [placed[ch] for ch in KEY_ORDER]

    if result is not None and result.solvable:
        gained = [result.collected.get("key_" + ch, 0) for ch in KEY_ORDER]
        carried = list(result.hero.keys)
        spent = [entry.keys[i] + gained[i] - carried[i]
                 for i in range(TOTAL_KEYS)]
        status = "solver OK"
    else:
        gained = placed_vec
        carried = list(entry.keys)
        spent = [0, 0, 0]
        status = (f"solver FAILED on floor {failed_at + 1}"
                  if failed_at is not None else "solver FAILED")

    return {
        "entry": list(entry.keys),
        "gained": gained,
        "spent": spent,
        "carried": carried,
        "placed": placed_vec,
        "status": status,
    }


def format_keys(values):
    return " ".join(f"{ch}{value}" for ch, value in zip(KEY_ORDER, values))


def secret_sheet(model, nodes, start):
    rows = model["rows"]
    secrets = [(x, y) for y, row in enumerate(rows)
               for x, ch in enumerate(row) if ch == gen_tower.SECRET]
    if not secrets:
        return "- **Secret:** none."
    parts = []
    for cell in secrets:
        closed = reach(nodes, start, {cell})
        opened = reach(nodes, start)
        new_cells = opened - closed
        rewards = sorted((x, y, rows[y][x]) for x, y in new_cells
                         if rows[y][x] in PICKUPS)
        reward = (", ".join(f"{ch}({x},{y})" for x, y, ch in rewards)
                  if rewards else "none (only the dead-end cell opens)")
        parts.append(f"`%` at ({cell[0]},{cell[1]}) -> {reward}")
    return "- **Secret:** " + "; ".join(parts) + "."


def enemy_sheets(model, nodes, arts, start, goal):
    rows = model["rows"]
    enemies = [(x, y, rows[y][x]) for y, row in enumerate(rows)
               for x, ch in enumerate(row) if ch in gen_tower.ENEMY]
    forced = []
    avoidable = []
    for x, y, ch in sorted(enemies, key=lambda e: (e[1], e[0])):
        cell = (x, y)
        if cell not in arts:
            avoidable.append(f"{ch}({x},{y})")
            continue
        gated = nodes - reach(nodes, start, {cell})
        pickups = sorted((xx, yy, rows[yy][xx]) for xx, yy in gated
                         if rows[yy][xx] in PICKUPS)
        gates_exit = goal not in reach(nodes, start, {cell})
        where = "gates the exit" if gates_exit else "gates a branch"
        if pickups:
            loot = ", ".join(f"{c}({xx},{yy})" for xx, yy, c in pickups)
            forced.append(f"{ch}({x},{y}) - {where}; loot behind: {loot}")
        else:
            forced.append(f"{ch}({x},{y}) - {where}; no pickups behind")
    lines = []
    if forced:
        lines.append("- **Articulation enemies (all routes cross):**")
        lines.extend(f"  - {item}" for item in forced)
    else:
        lines.append("- **Articulation enemies (all routes cross):** none.")
    if avoidable:
        lines.append("- **Avoidable enemies (deletion candidates):** " +
                     ", ".join(avoidable) + ".")
    else:
        lines.append("- **Avoidable enemies (deletion candidates):** none.")
    return lines


def dead_end_sheet(nodes):
    paths = sorted(dead_end_paths(nodes), key=len, reverse=True)
    long_paths = [path for path in paths if len(path) > 3]
    if long_paths:
        rendered = []
        for path in long_paths:
            coords = " -> ".join(f"({x},{y})" for x, y in path)
            rendered.append(f"{len(path)} cells: {coords}")
        return "- **Dead-end corridors >3 cells:** " + "; ".join(rendered) + "."
    if paths:
        longest = paths[0]
        coords = " -> ".join(f"({x},{y})" for x, y in longest)
        return (f"- **Dead-end corridors >3 cells:** none "
                f"(longest dead end: {len(longest)} cells, {coords}).")
    return "- **Dead-end corridors >3 cells:** none."


def build_sheet(index: int) -> str:
    model = cls(index)
    rows = model["rows"]
    nodes = nodes_of(rows)
    start = find_char(rows, "D")
    # Every region's last floor ends on 'G'; the other floors end on 'U'.
    goal_char = "G" if find_char(rows, "G") else "U"
    goal = find_char(rows, goal_char)
    if start is None or goal is None:
        raise SystemExit(f"floor {index + 1}: missing D or {goal_char}")
    arts = articulation_points(nodes)
    flow = key_flow(model)
    mandatory = mandatory_doors(nodes, start, goal, rows)
    mandatory_count = Counter(ch for _x, _y, ch in mandatory)
    total_doors = Counter(ch for row in rows for ch in row
                          if ch in gen_tower.DOOR)
    region = REGION_LABELS[model["region"]] if model["region"] < 5 else "?"
    lines = [
        f"### Floor {index + 1} - Region {region} - {model['role']} (draft)",
        "",
        "- **Key flow:** "
        f"entry {format_keys(flow['entry'])} -> "
        f"gained {format_keys(flow['gained'])} "
        f"(placed {format_keys(flow['placed'])}) -> "
        f"spent {format_keys(flow['spent'])} "
        f"(mandatory Y{mandatory_count['Y']} B{mandatory_count['B']} "
        f"R{mandatory_count['R']}) -> "
        f"carried {format_keys(flow['carried'])} "
        f"[{flow['status']}].",
    ]
    lines.extend(enemy_sheets(model, nodes, arts, start, goal))
    lines.append(secret_sheet(model, nodes, start))
    lines.append(dead_end_sheet(nodes))
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Print the hand-editing design sheet for one floor.")
    parser.add_argument("--floor", type=int, required=True,
                        help="1-based floor index (1..50)")
    args = parser.parse_args()
    index = args.floor - 1
    if not 0 <= index < gen_tower.FLOORS:
        raise SystemExit("--floor must be between 1 and 50")
    print(build_sheet(index))


if __name__ == "__main__":
    main()
