"""Generate the 50-floor Magic Tower campaign and its C source.

Layout
------
Every floor is a 3x3 grid of rooms joined by a randomised spanning tree of
one-cell corridors.  The room graph is a tree, so the corridor cell between two
rooms is a cut vertex: a door or a guard placed there genuinely gates progress
instead of being walk-aroundable.

Floors grow by region, matching the viewport contract:

    region I   ( 1-10)  11x11
    region II  (11-20)  13x13
    region III (21-30)  15x15
    region IV  (31-40)  17x17
    region V   (41-50)  19x19

Lock and key contract
---------------------
Each path door gets its key placed in the region that is reachable *before*
that door (earlier path doors are treated as open, optional vault doors stay
shut).  The solver in ``tools/solve_tower.py`` then re-proves this on the
finished map; if a key turns out to be reachable only through its own door the
generator relocates it, and as a last resort drops the offending door.  Floors
that cannot be repaired are rebuilt from a fresh layout, so generation fails
loudly instead of emitting a softlock.

Balance schedule (per region of ten floors)
-------------------------------------------
  * 14 attack gems, 11 defence gems  (=> ~70 / ~55 for the campaign)
  * 40 normal + 10 big potions       (tuned by the solver / test margin)
  * 18 path doors + 16 optional vault doors; 32 keys (red keys < red doors)
  * every non-boss floor gets at least one forced guard; boss floors (the last
    of each region) are sparse and carry no gems
  * object density among non-wall cells is filled to ~44% on non-boss floors

Enemy combat uses the one scaling definition in this module:

    scaled = base * (100 + 10 * floor_index) // 100

The C model is expected to implement exactly this integer rule.
"""
from __future__ import annotations

import math
import random
import sys
import re
import sys
from collections import Counter, deque
from dataclasses import dataclass, field
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

PROJECT_ROOT = Path(__file__).resolve().parents[1]

FLOORS = 50
REGION = 10
REGION_SIZES = (11, 13, 15, 17, 19)
DEFAULT_SEED = 20261018

WALL, FLOOR = "#", "."
KEY, DOOR = "ybr", "YBR"
POTION, BIG, ATK, DEF = "p", "P", "a", "d"
SHOP, NPC, UP, DOWN, GOAL = "S", "n", "U", "D", "G"
SECRET = "%"

# The single enemy-stat table.  Gold never scales.
ENEMY = {
    "s": (24, 6, 2, 3),
    "v": (35, 11, 4, 6),
    "k": (70, 20, 8, 12),
    "m": (120, 22, 10, 18),
    "g": (180, 30, 16, 26),
    "w": (260, 36, 22, 34),
    "M": (700, 58, 46, 150),
}
ENEMY_ORDER = "svkmgwM"

LADDER = [
    ("s", "v", "k"), ("v", "k", "m"), ("k", "m", "g"),
    ("m", "g", "w"), ("g", "w", "w"),
]

# Per-floor schedules.  Floor index 9 of each region is the sparse boss floor.
PATH_DOORS = 2
ATTACK_SCHEDULE = (2, 1, 2, 1, 2, 1, 2, 1, 2)          # 14 per region
DEFENCE_SCHEDULE = (1, 1, 1, 1, 2, 1, 1, 1, 2)         # 11 per region
POTION_SCHEDULE = (5, 4, 4, 5, 4, 4, 5, 4, 4)          # 39 per region
BIG_POTION_SCHEDULE = (1, 1, 1, 1, 1, 1, 1, 1, 1)      # 9 per region
RED_DOOR_SCHEDULE = (1, 1, 0, 1, 0, 1, 0, 1, 1)        # 6 per region
BLUE_DOOR_SCHEDULE = (1, 0, 1, 0, 1, 0, 1, 1, 1)       # 6 per region
YELLOW_DOOR_SCHEDULE = (0, 1, 0, 1, 0, 1, 0, 0, 1)     # 4 per region
RED_KEY_TOTAL = 4                                      # < 6 red doors / region
DENSITY_TARGET = 0.44
DENSITY_FLOOR = 0.40

START_HP = 100
START_ATK = 12
START_DEF = 4
GUARD_DAMAGE_TARGET = 0.07
BOSS_GUARD_DAMAGE_TARGET = 0.12


def floor_size(floor: int) -> int:
    return REGION_SIZES[floor // REGION]


def region_of(floor: int) -> int:
    return floor // REGION


def scaled_enemy(kind: str, floor: int) -> tuple[int, int, int, int]:
    """The one enemy-scaling definition shared with the Python solver.

    scaled = base * (100 + 10 * floor) // 100, for hp / attack / defence.
    Gold is deliberately unscaled.
    """
    hp, attack, defence, gold = ENEMY[kind]
    scale = 100 + 10 * floor
    return (hp * scale // 100, attack * scale // 100,
            defence * scale // 100, gold)


# ---------------------------------------------------------------------------
# geometry


@dataclass
class Room:
    x: int
    y: int
    w: int
    h: int

    @property
    def centre(self) -> tuple[int, int]:
        return (self.x + self.w // 2, self.y + self.h // 2)

    def contains(self, x: int, y: int) -> bool:
        return self.x <= x < self.x + self.w and self.y <= y < self.y + self.h

    def cells(self):
        for yy in range(self.y, self.y + self.h):
            for xx in range(self.x, self.x + self.w):
                yield (xx, yy)


class Grid:
    def __init__(self, size: int) -> None:
        self.size = size
        self.c = [[WALL] * size for _ in range(size)]

    def get(self, x: int, y: int) -> str:
        return self.c[y][x]

    def put(self, x: int, y: int, tile: str) -> None:
        self.c[y][x] = tile

    def carve(self, x: int, y: int) -> None:
        if 0 <= x < self.size and 0 <= y < self.size:
            self.c[y][x] = FLOOR

    def rows(self) -> list[str]:
        return ["".join(row) for row in self.c]

    def free(self) -> list[tuple[int, int]]:
        return [(x, y) for y in range(self.size) for x in range(self.size)
                if self.c[y][x] == FLOOR]

    def objects(self) -> int:
        return sum(1 for row in self.c for ch in row if ch not in (WALL, FLOOR))

    def walkable(self) -> int:
        return sum(1 for row in self.c for ch in row if ch != WALL)


class GenerationError(RuntimeError):
    pass


def _neighbours(cell: tuple[int, int]):
    x, y = cell
    return ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1))


def spanning_tree(rng: random.Random) -> list[tuple[tuple[int, int], tuple[int, int]]]:
    cells = [(sx, sy) for sy in range(3) for sx in range(3)]
    start = rng.choice(cells)
    inside = {start}
    edges = []
    while len(inside) < len(cells):
        options = []
        for a in sorted(inside):
            for b in _neighbours(a):
                if b in cells and b not in inside:
                    options.append((a, b))
        if not options:
            raise GenerationError("disconnected sector grid")
        a, b = rng.choice(options)
        inside.add(b)
        edges.append((a, b))
    return edges


def layout(size: int, rng: random.Random):
    pitch = (size - 2) // 3
    side = pitch - 1
    leftover = (size - 2) - 3 * pitch
    base = 1 + leftover // 2
    rooms = {(sx, sy): Room(base + sx * pitch, base + sy * pitch, side, side)
             for sy in range(3) for sx in range(3)}
    grid = Grid(size)
    for room in rooms.values():
        for x, y in room.cells():
            grid.carve(x, y)
    tree = spanning_tree(rng)
    for a, b in tree:
        for x, y in link_cells(rooms[a], rooms[b]):
            grid.carve(x, y)
    return grid, rooms, tree


def link_cells(a: Room, b: Room) -> list[tuple[int, int]]:
    """The L-shaped corridor cells between two room centres."""
    ax, ay = a.centre
    bx, by = b.centre
    cells = []
    for x in range(min(ax, bx), max(ax, bx) + 1):
        cells.append((x, ay))
    for y in range(min(ay, by), max(ay, by) + 1):
        cells.append((bx, y))
    out = []
    for cell in cells:
        if cell not in out:
            out.append(cell)
    return out


def link_connectors(a: Room, b: Room) -> list[tuple[int, int]]:
    """Corridor cells outside both rooms; a 1-cell gap in practice."""
    return [(x, y) for x, y in link_cells(a, b)
            if not a.contains(x, y) and not b.contains(x, y)]


def room_path(tree, start, goal):
    adjacency = {}
    for a, b in tree:
        adjacency.setdefault(a, []).append(b)
        adjacency.setdefault(b, []).append(a)
    parent = {start: None}
    queue = deque([start])
    while queue:
        room = queue.popleft()
        if room == goal:
            break
        for nxt in sorted(adjacency.get(room, [])):
            if nxt not in parent:
                parent[nxt] = room
                queue.append(nxt)
    if goal not in parent:
        raise GenerationError("rooms not connected")
    path = [goal]
    while path[-1] != start:
        path.append(parent[path[-1]])
    path.reverse()
    return path


def choose_start_goal(tree, rng):
    rooms = sorted({room for edge in tree for room in edge})
    distances = {}
    for room in rooms:
        seen = {room: 0}
        queue = deque([room])
        while queue:
            current = queue.popleft()
            for a, b in tree:
                nxt = b if a == current else a if b == current else None
                if nxt is not None and nxt not in seen:
                    seen[nxt] = seen[current] + 1
                    queue.append(nxt)
        distances[room] = seen
    farthest = max(max(dist.values()) for dist in distances.values())
    if farthest < 3:
        raise GenerationError("room tree too shallow for three gates")
    pairs = [(a, b) for a in rooms for b in rooms
             if a < b and distances[a].get(b) == farthest]
    return rng.choice(pairs)


# ---------------------------------------------------------------------------
# per-floor plan


@dataclass
class FloorPlan:
    floor: int
    boss: bool
    path_door_colours: list[str]
    optional_doors: list[tuple[str, bool]]  # (colour, has key)
    attack: int
    defence: int
    potions: int
    big_potions: int
    guards: int


def red_doors_before(index: int) -> int:
    return sum(RED_DOOR_SCHEDULE[:index])


def make_plan(floor: int, rng: random.Random) -> FloorPlan:
    index = floor % REGION
    boss = index == REGION - 1
    if boss:
        return FloorPlan(floor, True, [], [], 0, 0, 0, 1, 1)
    path_colours = ["Y", "Y"] if index == 0 else ["Y", "B"]
    optional: list[tuple[str, bool]] = []
    # Red vaults: only the first RED_KEY_TOTAL red doors of the region are
    # keyed, so red keys always trail red doors campaign-wide.
    before = red_doors_before(index)
    for j in range(RED_DOOR_SCHEDULE[index]):
        optional.append(("R", before + j < RED_KEY_TOTAL))
    for _ in range(BLUE_DOOR_SCHEDULE[index]):
        optional.append(("B", True))
    for _ in range(YELLOW_DOOR_SCHEDULE[index]):
        optional.append(("Y", True))
    guards = 1 + (1 if rng.random() < 0.35 else 0)
    return FloorPlan(
        floor=floor,
        boss=boss,
        path_door_colours=path_colours,
        optional_doors=optional,
        attack=ATTACK_SCHEDULE[index],
        defence=DEFENCE_SCHEDULE[index],
        potions=POTION_SCHEDULE[index],
        big_potions=BIG_POTION_SCHEDULE[index],
        guards=guards,
    )


# ---------------------------------------------------------------------------
# reachability used by the generator (all enemies are assumed beatable)


def grid_reach(grid: Grid, start, open_doors: set[tuple[int, int]]):
    blocked = {(x, y) for y in range(grid.size) for x in range(grid.size)
               if grid.get(x, y) in DOOR and (x, y) not in open_doors}
    return _grid_flood(grid, start, blocked)


def _grid_flood(grid: Grid, start, blocked):
    size = grid.size
    dist = {start: 0}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        for nx, ny in _neighbours((x, y)):
            if not (0 <= nx < size and 0 <= ny < size):
                continue
            cell = (nx, ny)
            if cell in dist or cell in blocked:
                continue
            if grid.get(nx, ny) == WALL:
                continue
            dist[cell] = dist[(x, y)] + 1
            queue.append(cell)
    return dist


def reach_with_decoys_blocked(grid: Grid, start, decoys: set[tuple[int, int]],
                              doors_open: bool = True):
    blocked = set() if doors_open else {
        (x, y) for y in range(grid.size) for x in range(grid.size)
        if grid.get(x, y) in DOOR}
    blocked |= decoys
    return _grid_flood(grid, start, blocked)


# ---------------------------------------------------------------------------
# placement helpers


def free_cells(grid: Grid, reachable, reserved: set[tuple[int, int]]):
    return [(x, y) for x, y in sorted(reachable)
            if grid.get(x, y) == FLOOR and (x, y) not in reserved]


def place_one(grid: Grid, candidates, tile: str, reserved) -> tuple[int, int]:
    if not candidates:
        raise GenerationError("no free cell for tile " + tile)
    cell = candidates.pop()
    grid.put(cell[0], cell[1], tile)
    reserved.add(cell)
    return cell


def shuffle_take(rng: random.Random, cells):
    cells = list(cells)
    rng.shuffle(cells)
    return cells


# ---------------------------------------------------------------------------
# enemy selection


def choose_guard(hero, floor: int, rng: random.Random, boss: bool = False) -> str:
    """Pick a beatable enemy whose damage is a useful slice of current HP."""
    from solve_tower import fight_preview  # lazy: solve_tower imports us

    region = region_of(floor)
    kinds = sorted(set(LADDER[region]))
    viable = []
    for kind in kinds:
        preview = fight_preview(hero, kind, floor)
        if preview is None:
            continue
        damage = preview[0]
        if damage <= hero.hp * 0.22:
            viable.append((kind, damage))
    if not viable:
        # Nothing on the ladder is beatable: fall back to the weakest scaled
        # slime; the repair loop will weaken it further if even that fails.
        return "s"
    target = hero.hp * (BOSS_GUARD_DAMAGE_TARGET if boss
                        else GUARD_DAMAGE_TARGET)
    viable.sort(key=lambda item: (abs(item[1] - target), item[1]))
    return viable[0][0]


def choose_decoy(floor: int, rng: random.Random) -> str:
    region = region_of(floor)
    ladder = LADDER[region]
    return rng.choice(ladder)


def articulation_points(grid: Grid) -> set[tuple[int, int]]:
    """Cut vertices of the non-wall graph (brute force; floors are small)."""
    nodes = [(x, y) for y in range(grid.size) for x in range(grid.size)
             if grid.get(x, y) != WALL]
    node_set = set(nodes)
    if len(nodes) <= 2:
        return set(nodes)
    points = set()
    for removed in nodes:
        start = next(cell for cell in nodes if cell != removed)
        seen = {start}
        queue = deque([start])
        while queue:
            x, y = queue.popleft()
            for nx, ny in _neighbours((x, y)):
                cell = (nx, ny)
                if cell == removed or cell in seen or cell not in node_set:
                    continue
                seen.add(cell)
                queue.append(cell)
        if len(seen) != len(nodes) - 1:
            points.add(removed)
    return points


# ---------------------------------------------------------------------------
# floor assembly


def build_floor(floor: int, hero, rng: random.Random):
    from solve_tower import analyze_floor

    last_result = None
    last_error = None
    for attempt in range(80):
        try:
            grid, rooms, tree = layout(floor_size(floor), rng)
            reserve: set[tuple[int, int]] = set()
            start_room, goal_room = choose_start_goal(tree, rng)
            path = room_path(tree, start_room, goal_room)
            path_edges = list(zip(path, path[1:]))

            # Down stair in the start room, exit in the goal room.
            start = rng.choice(sorted(rooms[start_room].cells()))
            goal = rng.choice(sorted(rooms[goal_room].cells()))
            if start == goal:
                raise GenerationError("stairs overlap")
            grid.put(*start, DOWN)
            grid.put(*goal, GOAL if floor == FLOORS - 1 else UP)
            reserve.update((start, goal))

            plan = make_plan(floor, rng)
            place_path_gates(grid, rooms, tree, path_edges, start, plan, hero,
                             rng, reserve)
            main_reach, door_entries, path_doors = place_optional_doors(
                grid, rooms, tree, path_edges, start, plan, reserve, rng)

            place_keys_and_items(grid, plan, start, main_reach, door_entries,
                                 path_doors, reserve, rng)
            decorate(grid, floor, plan, start, goal, reserve, rng)
            trim_crowded(grid, plan)

            rows = grid.rows()
            result = analyze_floor(rows, floor, hero)
            result = repair_floor(rows, floor, hero, plan, result, rng)
            if result is None:
                continue
            rows, result = result
            grid = Grid(len(rows[0]))
            grid.c = [list(row) for row in rows]

            if not plan.boss and density(grid) < DENSITY_FLOOR:
                continue
            if len(rows) != floor_size(floor):
                continue
            last_result = (rows, result)
            return rows, result
        except GenerationError as exc:
            last_error = exc
            continue
    raise GenerationError(f"floor {floor + 1} generation failed: {last_error}")


def density(grid: Grid) -> float:
    walkable = grid.walkable()
    return grid.objects() / walkable if walkable else 0.0


def place_path_gates(grid, rooms, tree, path_edges, start, plan, hero, rng,
                     reserve):
    """Place the two mandatory doors and one or two forced guards."""
    edge_count = len(path_edges)
    door_count = len(plan.path_door_colours)
    if door_count > PATH_DOORS:
        raise GenerationError("too many path doors")
    guards = min(plan.guards, edge_count - door_count)
    gate_count = door_count + guards
    if gate_count < 1:
        raise GenerationError("no room for path gates")
    if gate_count == 1:
        indices = [edge_count // 2] if edge_count > 1 else [0]
    elif gate_count >= edge_count:
        indices = list(range(edge_count))
    else:
        indices = sorted({round(i * (edge_count - 1) / (gate_count - 1))
                          for i in range(gate_count)})
        cursor = 0
        while len(indices) < gate_count:
            if cursor not in indices:
                indices.append(cursor)
            cursor += 1
        indices = sorted(indices)

    # Spread the mandatory doors across the route and fill the rest with
    # guards.  A boss floor has zero doors and therefore all guards.
    gate_types = ["guard"] * gate_count
    if door_count:
        if door_count == 1:
            door_slots = [0]
        else:
            door_slots = sorted({round(i * (gate_count - 1) / (door_count - 1))
                                 for i in range(door_count)})
            cursor = 0
            while len(door_slots) < door_count:
                if cursor not in door_slots:
                    door_slots.append(cursor)
                cursor += 1
            door_slots = sorted(door_slots)
        for slot in door_slots:
            gate_types[slot] = "door"

    open_doors: set[tuple[int, int]] = set()
    door_index = 0
    for edge_index, gate_type in zip(indices, gate_types):
        a, b = path_edges[edge_index]
        connectors = link_connectors(rooms[a], rooms[b])
        if not connectors:
            raise GenerationError("path edge has no connector")
        cell = rng.choice(sorted(connectors))
        if grid.get(*cell) != FLOOR:
            raise GenerationError("connector already used")
        if gate_type == "door":
            colour = plan.path_door_colours[door_index]
            door_index += 1
            grid.put(*cell, colour)
            # Key goes in the region reachable *before* this door.  Guards and
            # earlier path doors are treated as passable.
            reach = grid_reach(grid, start, open_doors)
            candidates = free_cells(grid, reach, reserve)
            if not candidates:
                raise GenerationError("no room for a path key")
            place_one(grid, shuffle_take(rng, candidates), KEY[DOOR.index(colour)],
                      reserve)
            open_doors.add(cell)
        else:
            kind = choose_guard(hero, plan.floor, rng)
            grid.put(*cell, kind)
        reserve.add(cell)


def place_optional_doors(grid, rooms, tree, path_edges, start, plan, reserve,
                         rng):
    """Put vault doors on branch corridors, then return the main reach.

    Returns (main_reach, [(door_cell, has_key)], path_door_cells).
    """
    path_rooms = {room for edge in path_edges for room in edge}
    path_set = {tuple(sorted(edge)) for edge in path_edges}
    branch_edges = [edge for edge in tree if tuple(sorted(edge)) not in path_set]
    # Prefer branch corridors that touch the main route: their vaults are
    # immediately behind one door rather than nested behind another vault.
    front = [e for e in branch_edges if e[0] in path_rooms or e[1] in path_rooms]
    rest = [e for e in branch_edges if e not in front]
    rng.shuffle(front)
    rng.shuffle(rest)
    branch_edges = front + rest
    if len(plan.optional_doors) > len(branch_edges):
        raise GenerationError("not enough branch corridors for vaults")
    entries = []
    for (colour, has_key), (a, b) in zip(plan.optional_doors, branch_edges):
        connectors = link_connectors(rooms[a], rooms[b])
        if not connectors:
            raise GenerationError("branch edge has no connector")
        cell = rng.choice(sorted(connectors))
        if grid.get(*cell) != FLOOR:
            raise GenerationError("branch connector already used")
        grid.put(*cell, colour)
        reserve.add(cell)
        entries.append((cell, has_key))
    path_doors = _path_door_cells(grid, rooms, tree, path_edges)
    main_reach = grid_reach(grid, start, path_doors)
    return main_reach, entries, path_doors


def _path_door_cells(grid, rooms, tree, path_edges):
    cells = set()
    for a, b in path_edges:
        for x, y in link_connectors(rooms[a], rooms[b]):
            if grid.get(x, y) in DOOR:
                cells.add((x, y))
    return cells


def vault_cells(grid, start, path_doors, door_cell):
    """Cells that become reachable only after the given vault door opens."""
    open_doors = set(path_doors) | {door_cell}
    reach = grid_reach(grid, start, open_doors)
    main = grid_reach(grid, start, set(path_doors))
    return sorted(set(reach) - set(main))


def place_keys_and_items(grid, plan, start, main_reach, door_entries,
                         path_doors, reserve, rng):
    main_free = free_cells(grid, main_reach, reserve)
    rng.shuffle(main_free)
    if not main_free:
        raise GenerationError("empty main region")

    # Optional vault keys first: a key must live outside the vault it opens.
    for colour, has_key in plan.optional_doors:
        if not has_key:
            continue
        if not main_free:
            raise GenerationError("no room for a vault key")
        place_one(grid, main_free, KEY[DOOR.index(colour)], reserve)
    main_free = [cell for cell in main_free if grid.get(*cell) == FLOOR]

    # Rewards behind vault doors: a keyed vault pays a gem (or potion); an
    # unkeyed red vault hides a potion that the deliberate key shortfall
    # leaves behind.
    for door_cell, has_key in door_entries:
        cells = vault_cells(grid, start, path_doors, door_cell)
        free = [cell for cell in cells if grid.get(*cell) == FLOOR]
        rng.shuffle(free)
        if not free:
            continue
        if has_key and plan.attack > 0:
            place_one(grid, free, ATK, reserve)
            plan.attack -= 1
        elif has_key and plan.defence > 0:
            place_one(grid, free, DEF, reserve)
            plan.defence -= 1
        elif plan.potions > 0:
            place_one(grid, free, POTION, reserve)
            plan.potions -= 1

    main_free = [cell for cell in main_free if grid.get(*cell) == FLOOR]
    gems = [ATK] * plan.attack + [DEF] * plan.defence
    rng.shuffle(gems)
    for tile in gems:
        place_one(grid, main_free, tile, reserve)
    potions = [POTION] * plan.potions + [BIG] * plan.big_potions
    rng.shuffle(potions)
    for tile in potions:
        place_one(grid, main_free, tile, reserve)

    # NPC every floor, merchant on the fifth floor of each region.
    main_free = [cell for cell in main_free if grid.get(*cell) == FLOOR]
    if main_free and plan.floor % REGION != REGION - 1:
        place_one(grid, main_free, NPC, reserve)
    if plan.floor % REGION == 5 and main_free:
        place_one(grid, main_free, SHOP, reserve)


def trim_crowded(grid, plan):
    """Keep non-boss density inside the 40-50% design band.

    Small floors (11x11, 13x13) have only ~45 walkable cells, so the mandatory
    key/door schedule plus a merchant can push a few floors over 50%.  Flavour
    objects go first; keys, gems, potions and enemies are never removed.
    """
    if plan.boss or density(grid) <= 0.50:
        return
    for tile in (NPC, SECRET):
        if density(grid) <= 0.50:
            return
        for y in range(grid.size):
            for x in range(grid.size):
                if grid.get(x, y) == tile:
                    grid.put(x, y, FLOOR)
                    break
            else:
                continue
            break


def decorate(grid, floor, plan, start, goal, reserve, rng):
    # Secret dead-end: a wall with exactly one non-wall neighbour.
    secret = None
    size = grid.size
    for y in range(size):
        for x in range(size):
            if grid.get(x, y) != WALL:
                continue
            open_sides = sum(1 for nx, ny in _neighbours((x, y))
                             if 0 <= nx < size and 0 <= ny < size
                             and grid.get(nx, ny) != WALL)
            if open_sides == 1:
                secret = (x, y)
                break
        if secret:
            break
    if secret:
        grid.put(*secret, SECRET)

    if plan.boss:
        # One boss monster plus a couple of decorative guards; no gems, no
        # density padding.  Boss floors stay deliberately sparse.
        points = articulation_points(grid)
        cells = [cell for cell in grid.free() if cell not in reserve
                 and cell not in points]
        rng.shuffle(cells)
        if cells:
            grid.put(*cells.pop(), "M")
        for _ in range(2):
            if not cells:
                break
            grid.put(*cells.pop(), choose_decoy(floor, rng))
        return

    target = math.ceil(DENSITY_TARGET * grid.walkable())
    points = articulation_points(grid)
    decoys: set[tuple[int, int]] = set()
    for _pass in range(3):
        if grid.objects() >= target:
            break
        candidates = [cell for cell in grid.free()
                      if cell not in reserve and cell not in points]
        rng.shuffle(candidates)
        for cell in candidates:
            if grid.objects() >= target:
                break
            grid.put(*cell, choose_decoy(floor, rng))
            decoys.add(cell)
            reserve.add(cell)
        prune_decoys(grid, start, goal, decoys, reserve)
        # Recompute cut vertices after removals only if we need another pass.
    prune_decoys(grid, start, goal, decoys, reserve)


def prune_decoys(grid, start, goal, decoys, reserve):
    """Remove decorative enemies that accidentally seal a key or the exit."""
    important = set()
    for y in range(grid.size):
        for x in range(grid.size):
            if grid.get(x, y) in KEY + POTION + BIG + ATK + DEF:
                important.add((x, y))
    important.add(goal)
    important.add(start)
    while True:
        reach = reach_with_decoys_blocked(grid, start, decoys)
        missing = [cell for cell in important if cell not in reach]
        if not missing:
            return
        frontier = [cell for cell in decoys
                    if any(nb in reach for nb in _neighbours(cell))]
        if not frontier:
            # Remove the decoy closest to a missing cell.
            if not decoys:
                return
            missing_cell = missing[0]
            frontier = [min(decoys, key=lambda c: abs(c[0] - missing_cell[0]) +
                            abs(c[1] - missing_cell[1]))]
        victim = min(frontier)
        decoys.discard(victim)
        reserve.discard(victim)
        grid.put(victim[0], victim[1], FLOOR)


# ---------------------------------------------------------------------------
# repair


def repair_floor(rows, floor, hero, plan, result, rng):
    """Relocate self-dependent keys / drop blocking doors; None means rebuild."""
    from solve_tower import analyze_floor, reach_without_colour

    if result.solvable and not result.self_dependent and not result.unreachable_keys:
        return rows, result

    rows = list(rows)
    for _round in range(40):
        result = analyze_floor(rows, floor, hero)
        if result.solvable and not result.self_dependent and not result.unreachable_keys:
            return rows, result

        # 1) Self-dependent keys: move the key into the area that is provably
        #    reachable without opening a door of its own colour.
        moved = False
        start = _find_down(rows)
        if start is not None:
            for colour, cells in result.self_dependent.items():
                allowed = reach_without_colour(rows, start, hero.keys, colour)
                for cell in cells:
                    target = _free_target(rows, allowed, {start})
                    if target is None:
                        continue
                    x, y = cell
                    rows[y] = rows[y][:x] + FLOOR + rows[y][x + 1:]
                    tx, ty = target
                    rows[ty] = rows[ty][:tx] + KEY[colour] + rows[ty][tx + 1:]
                    moved = True
                    break
                if moved:
                    break
        if moved:
            continue

        # 2) A door that cannot be passed: drop it.
        if result.blocker and result.blocker[0] == "door":
            cell = result.blocker[1]
            x, y = cell
            if rows[y][x] in DOOR:
                rows[y] = rows[y][:x] + FLOOR + rows[y][x + 1:]
                continue

        # 3) An enemy that cannot be beaten: weaken or remove it.
        if result.blocker and result.blocker[0] == "enemy":
            cell = result.blocker[1]
            x, y = cell
            kind = rows[y][x]
            if kind in ENEMY:
                weaker = _weaker(kind)
                rows[y] = rows[y][:x] + weaker + rows[y][x + 1:]
                continue

        # 4) Fallback: drop the last still-closed door on the map, then any
        #    non-slime guard, then give up and rebuild.
        doors = [(x, y) for y, row in enumerate(rows)
                 for x, ch in enumerate(row) if ch in DOOR]
        if doors:
            x, y = doors[-1]
            rows[y] = rows[y][:x] + FLOOR + rows[y][x + 1:]
            continue
        guards = [(x, y) for y, row in enumerate(rows)
                  for x, ch in enumerate(row) if ch in ENEMY and ch != "s"]
        if guards:
            x, y = guards[-1]
            rows[y] = rows[y][:x] + "s" + rows[y][x + 1:]
            continue
        return None
    return None


def _find_down(rows):
    for y, row in enumerate(rows):
        x = row.find(DOWN)
        if x >= 0:
            return (x, y)
    return None


def _free_target(rows, allowed, forbidden):
    for cell in sorted(allowed):
        x, y = cell
        if cell in forbidden:
            continue
        if rows[y][x] == FLOOR:
            return cell
    return None


def _weaker(kind: str) -> str:
    index = ENEMY_ORDER.index(kind)
    return ENEMY_ORDER[index - 1] if index > 0 else FLOOR


# ---------------------------------------------------------------------------
# campaign


def build_campaign(seed: int = DEFAULT_SEED, verbose: bool = False):
    from solve_tower import Hero

    rng = random.Random(seed)
    hero = Hero()
    floors = []
    results = []
    for floor in range(FLOORS):
        rows, result = build_floor(floor, hero, rng)
        floors.append(rows)
        results.append(result)
        hero = result.hero
        if verbose:
            print(f"  built {floor + 1:2d}F {len(rows[0])}x{len(rows)} "
                  f"hp={hero.hp} atk={hero.attack} def={hero.defence} "
                  f"density={result.density:.1%} "
                  f"selfdep={result.self_dependent or '-'}")
        if not result.solvable:
            raise GenerationError(f"floor {floor + 1} unsolvable after repair")
    return floors, results


# ---------------------------------------------------------------------------
# C emission / reading


def emit_c(floors: list[list[str]]) -> str:
    lines = [
        "/* Generated by tools/gen_tower.py -- do not edit by hand. */",
        '#include "tower_floors.h"',
        "",
    ]
    names = []
    for index, rows in enumerate(floors):
        name = f"tower_floor_{index + 1:02d}_rows"
        names.append(name)
        region = "I II III IV V".split()[index // REGION]
        lines.append(f"/* {index + 1}F -- region {region}, "
                     f"{len(rows[0])}x{len(rows)} */")
        lines.append(f"static const char *const {name}[] = {{")
        for row in rows:
            lines.append(f'    "{row}",')
        lines.append("};")
        lines.append("")
    lines.append("const tower_floor_desc_t tower_floor_table[TOWER_FLOORS] = {")
    for index, rows in enumerate(floors):
        region = "I II III IV V".split()[index // REGION]
        lines.append(f"    {{ {len(rows[0])}, {len(rows)}, {names[index]} }},"
                     f" /* {index + 1:2d}F region {region} */")
    lines.append("};")
    return "\n".join(lines) + "\n"


def write_c(floors: list[list[str]], path: Path) -> None:
    path.write_text(emit_c(floors), encoding="utf-8")


_ARRAY_RE = re.compile(
    r"static const char \*const (\w+)\[\] = \{(.*?)\n\};", re.S)
_ROW_RE = re.compile(r'"([^"]*)"')
_TABLE_RE = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*\}")


def read_generated_c(path: Path) -> list[list[str]]:
    source = path.read_text(encoding="utf-8")
    arrays = {name: _ROW_RE.findall(body)
              for name, body in _ARRAY_RE.findall(source)}
    entries = _TABLE_RE.findall(source)
    if len(entries) != FLOORS:
        raise ValueError(f"{path}: expected {FLOORS} table entries, "
                         f"found {len(entries)}")
    floors = []
    for width, height, name in entries:
        if name not in arrays:
            raise ValueError(f"{path}: missing row array {name}")
        rows = arrays[name]
        if len(rows) != int(height) or any(len(row) != int(width) for row in rows):
            raise ValueError(f"{path}: {name} dimensions do not match table")
        floors.append(rows)
    return floors


# ---------------------------------------------------------------------------
# CLI


def main() -> None:
    out_path = Path(__file__).resolve().parents[1] / "main" / "tower_floors.c"
    # Never clobber hand-authored floors. The campaign is being hand-polished
    # floor by floor; regeneration would silently destroy that work (audit
    # finding MINOR 16). Pass --force to regenerate everything anyway.
    if out_path.exists() and "hand-authored" in out_path.read_text(encoding="utf-8"):
        if "--force" not in sys.argv:
            raise SystemExit(
                "refusing to overwrite hand-authored floors in main/tower_floors.c; "
                "pass --force to regenerate the whole table")
    floors, results = build_campaign(DEFAULT_SEED, verbose=True)
    out = PROJECT_ROOT / "main" / "tower_floors.c"
    write_c(floors, out)
    print(f"wrote {out} ({len(floors)} floors)")

    counts = Counter()
    for rows in floors:
        counts.update(ch for row in rows for ch in row if ch in ENEMY)
    print("enemies:", dict(counts))
    print("deterministic seed:", DEFAULT_SEED)


if __name__ == "__main__":
    main()
