"""Softlock solver and campaign simulator for the generated Magic Tower maps.

The solver operates on the same tile alphabet the generator emits:

    '#' wall          '.' floor          'ybr' keys       'YBR' doors
    'p' potion        'P' big potion     'a' attack gem   'd' defence gem
    'svkmgwM' enemies 'S' shop           'n' npc          'U' up stair
    'D' down stair    'G' goal           '%' secret

A floor is solved from the down stair ('D').  The solver repeatedly:

  * floods the region reachable with the keys currently held (doors are walls
    until a matching key is spent on them; enemies are walls until they are
    beaten),
  * collects every reachable key / potion / gem on the way,
  * follows the cheapest route towards the up stair (or the goal on the last
    floor), paying for the first door or enemy barrier on that route,
  * when the exit is reachable, opens every remaining reachable door it can
    still afford to collect the vaults behind them.

The campaign simulator carries HP / keys / gold / turns between floors and
uses ``gen_tower.scaled_enemy`` (the single enemy-scaling definition) for
combat, so the Python simulation and the C model cannot drift apart.

The solver also reports keys that are only reachable through a door of their
own colour.  These are the classic softlock keys; the generator uses this
report to relocate them before a floor is accepted.

Run this file directly for a campaign report:

    python tools/solve_tower.py
"""
from __future__ import annotations

import heapq
import sys
from collections import Counter, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from gen_tower import (  # noqa: E402  (path bootstrap must run first)
    ATK,
    BIG,
    DEF,
    DOOR,
    ENEMY,
    FLOOR,
    GOAL,
    KEY,
    POTION,
    WALL,
    scaled_enemy,
)

FLOORS = 50
REGION = 10
REGION_SIZES = (11, 13, 15, 17, 19)

KEY_INDEX = {ch: i for i, ch in enumerate(KEY)}
DOOR_INDEX = {ch: i for i, ch in enumerate(DOOR)}
PICKUPS = frozenset(KEY + POTION + BIG + ATK + DEF)
NEIGHBOURS = ((1, 0), (-1, 0), (0, 1), (0, -1))
BARRIER_COST = 10000
START_HP = 100
START_ATK = 12
START_DEF = 4
POTION_HEAL = 30
BIG_POTION_HEAL = 80
ATK_GAIN = 5
DEF_GAIN = 3


# ---------------------------------------------------------------------------
# small data holders


@dataclass
class Hero:
    hp: int = START_HP
    attack: int = START_ATK
    defence: int = START_DEF
    gold: int = 0
    keys: list[int] = field(default_factory=lambda: [0, 0, 0])
    turns: int = 0

    def clone(self) -> "Hero":
        return Hero(self.hp, self.attack, self.defence, self.gold,
                    list(self.keys), self.turns)


@dataclass
class FloorResult:
    floor: int
    solvable: bool
    reached: bool
    entry: Hero
    hero: Hero
    reason: str = ""
    blocker: tuple | None = None
    damage: int = 0
    fights: int = 0
    turns: int = 0
    collected: Counter = field(default_factory=Counter)
    self_dependent: dict[int, list[tuple[int, int]]] = field(default_factory=dict)
    unreachable_keys: list[tuple[int, int, str]] = field(default_factory=list)
    enemies_total: int = 0
    walkable: int = 0
    objects: int = 0

    @property
    def density(self) -> float:
        return self.objects / self.walkable if self.walkable else 0.0


@dataclass
class RegionSummary:
    region: int
    floors: int
    enemies: int
    avg_density: float
    boss_density: float
    final_hp: int
    max_hp: int

    @property
    def hp_fraction(self) -> float:
        return self.final_hp / self.max_hp if self.max_hp else 0.0


@dataclass
class CampaignResult:
    completed: bool
    floors: list[FloorResult]
    hero: Hero
    max_hp: int
    regions: list[RegionSummary]
    failed_floor: int | None = None

    @property
    def margin(self) -> float:
        return self.hero.hp / self.max_hp if self.max_hp else 0.0


# ---------------------------------------------------------------------------
# grid helpers


def _dims(m: list[list[str]]) -> tuple[int, int]:
    return len(m[0]), len(m)


def _blocked_cells(m: list[list[str]]) -> set[tuple[int, int]]:
    """Doors and living enemies: passable only by spending or fighting."""
    w, h = _dims(m)
    blocked = set()
    for y in range(h):
        row = m[y]
        for x in range(w):
            ch = row[x]
            if ch in DOOR or ch in ENEMY:
                blocked.add((x, y))
    return blocked


def _find(m: list[list[str]], want: str) -> tuple[int, int] | None:
    for y, row in enumerate(m):
        x = row.find(want) if isinstance(row, list) else -1
        if x >= 0:
            return (x, y)
    return None


def _find_char(rows: Iterable[str], want: str) -> tuple[int, int] | None:
    for y, row in enumerate(rows):
        x = row.find(want)
        if x >= 0:
            return (x, y)
    return None


def _flood(m: list[list[str]], start: tuple[int, int],
           blocked: set[tuple[int, int]]):
    """BFS through every non-wall, non-blocked cell.  Returns dist + parent."""
    w, h = _dims(m)
    dist = {start: 0}
    parent: dict[tuple[int, int], tuple[int, int]] = {}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        base = dist[(x, y)]
        for dx, dy in NEIGHBOURS:
            nx, ny = x + dx, y + dy
            if not (0 <= nx < w and 0 <= ny < h):
                continue
            cell = (nx, ny)
            if cell in dist or cell in blocked:
                continue
            if m[ny][nx] == WALL:
                continue
            dist[cell] = base + 1
            parent[cell] = (x, y)
            queue.append(cell)
    return dist, parent


def _reconstruct(parent, start, goal):
    path = [goal]
    node = goal
    while node != start:
        node = parent.get(node)
        if node is None:
            return []
        path.append(node)
    path.reverse()
    return path


def _pickup(m: list[list[str]], cell, hero: Hero,
            collected: Counter) -> bool:
    x, y = cell
    ch = m[y][x]
    if ch in KEY:
        hero.keys[KEY_INDEX[ch]] += 1
        collected[f"key_{ch}"] += 1
    elif ch == POTION:
        hero.hp += POTION_HEAL
        collected["potion"] += 1
    elif ch == BIG:
        hero.hp += BIG_POTION_HEAL
        collected["big_potion"] += 1
    elif ch == ATK:
        hero.attack += ATK_GAIN
        collected["attack"] += 1
    elif ch == DEF:
        hero.defence += DEF_GAIN
        collected["defence"] += 1
    else:
        return False
    m[y][x] = FLOOR
    return True


def _sweep(m: list[list[str]], pos, hero: Hero, collected: Counter):
    """Walk to every reachable pickup (nearest first), collecting on the way.

    Returns the final position plus the flood of the remaining reachable area.
    """
    blocked = _blocked_cells(m)
    while True:
        dist, parent = _flood(m, pos, blocked)
        targets = [cell for cell in dist
                   if cell != pos and m[cell[1]][cell[0]] in PICKUPS]
        if not targets:
            return pos, dist, parent
        target = min(targets, key=lambda c: (dist[c], c[1], c[0]))
        path = _reconstruct(parent, pos, target)
        if not path:
            return pos, dist, parent
        for cell in path[1:]:
            _pickup(m, cell, hero, collected)
        hero.turns += len(path) - 1
        pos = target


# ---------------------------------------------------------------------------
# combat


def fight_preview(hero: Hero, kind: str, floor: int):
    """Return (damage, rounds, gold) or None when the enemy cannot be beaten.

    Enemy stats come exclusively from ``gen_tower.scaled_enemy``; the C model
    must implement the identical ``base * (100 + 10 * floor) / 100`` rule.
    """
    ehp, eatk, edef, gold = scaled_enemy(kind, floor)
    if hero.attack <= edef:
        return None
    hit = hero.attack - edef
    rounds = (ehp + hit - 1) // hit
    damage = (rounds - 1) * max(eatk - hero.defence, 0)
    if damage >= hero.hp:
        return None
    return damage, rounds, gold


# ---------------------------------------------------------------------------
# goal route with barriers


def _barrier_path(m: list[list[str]], sources: set[tuple[int, int]],
                  goal: tuple[int, int], hero: Hero, floor: int):
    """Dijkstra from any reachable cell to *goal*.

    Doors are passable only while a matching key is held and enemies only while
    the player can survive the fight.  Crossing a barrier is deliberately very
    expensive, so the solver only fights when there is no clear route.
    """
    w, h = _dims(m)
    dist: dict[tuple[int, int], int] = {}
    parent: dict[tuple[int, int], tuple[int, int]] = {}
    heap = []
    tie = 0
    for cell in sources:
        dist[cell] = 0
        heapq.heappush(heap, (0, tie, cell))
        tie += 1
    while heap:
        d, _, cell = heapq.heappop(heap)
        if d != dist.get(cell):
            continue
        if cell == goal:
            break
        x, y = cell
        for dx, dy in NEIGHBOURS:
            nx, ny = x + dx, y + dy
            if not (0 <= nx < w and 0 <= ny < h):
                continue
            nxt = (nx, ny)
            ch = m[ny][nx]
            if ch == WALL:
                continue
            cost = 1
            if ch in DOOR:
                if hero.keys[DOOR_INDEX[ch]] <= 0:
                    continue
                cost = BARRIER_COST + 1
            elif ch in ENEMY:
                preview = fight_preview(hero, ch, floor)
                if preview is None:
                    continue
                cost = BARRIER_COST + 1
            nd = d + cost
            if nd < dist.get(nxt, 1 << 60):
                dist[nxt] = nd
                parent[nxt] = cell
                heapq.heappush(heap, (nd, tie, nxt))
                tie += 1
    if goal not in dist:
        return None
    return _reconstruct_from_sources(parent, goal)


def _reconstruct_from_sources(parent, goal):
    path = [goal]
    node = goal
    while node in parent:
        node = parent[node]
        path.append(node)
    path.reverse()
    return path


def _execute_to_barrier(m: list[list[str]], path, hero: Hero, floor: int,
                        stats: Counter):
    """Walk along *path* until the first door / enemy, then stop.

    Returns (outcome, cell).  ``outcome`` is 'barrier', 'exit' or 'blocked'.
    """
    for cell in path[1:]:
        x, y = cell
        ch = m[y][x]
        if ch == WALL:
            return "blocked", cell
        if ch in DOOR:
            colour = DOOR_INDEX[ch]
            if hero.keys[colour] <= 0:
                return "blocked", cell
            hero.keys[colour] -= 1
            m[y][x] = FLOOR
            hero.turns += 1
            stats["doors"] += 1
            return "barrier", cell
        if ch in ENEMY:
            preview = fight_preview(hero, ch, floor)
            if preview is None:
                return "blocked", cell
            damage, _rounds, gold = preview
            hero.hp -= damage
            hero.gold += gold
            m[y][x] = FLOOR
            hero.turns += 1
            stats["fights"] += 1
            stats["damage"] += damage
            stats["killed_" + ch] += 1
            return "barrier", cell
        if ch == GOAL:
            # Never step onto the exit until the optional vaults are cleared.
            return "exit", cell
        _pickup(m, cell, hero, stats["collected"])
        hero.turns += 1
    return "blocked", path[-1]


def _clear_optional_doors(m: list[list[str]], pos, hero: Hero,
                          stats: Counter):
    """Open every still-affordable reachable door and loot behind it.

    Called only once the exit is already reachable: keys no longer have to be
    reserved for progression, so spending them on vaults is free value.
    """
    while True:
        pos, dist, _parent = _sweep(m, pos, hero, stats["collected"])
        best = None
        for (x, y), d in dist.items():
            for dx, dy in NEIGHBOURS:
                nx, ny = x + dx, y + dy
                if not (0 <= nx < len(m[0]) and 0 <= ny < len(m)):
                    continue
                ch = m[ny][nx]
                if ch in DOOR and hero.keys[DOOR_INDEX[ch]] > 0:
                    key = (d + 1, ny, nx)
                    if best is None or key < best:
                        best = key
        if best is None:
            return pos
        _d, dy, dx = best
        blocked = _blocked_cells(m)
        blocked.discard((dx, dy))
        reach, parent = _flood(m, pos, blocked)
        if (dx, dy) not in reach:
            return pos
        path = _reconstruct(parent, pos, (dx, dy))
        for cell in path[1:-1]:
            _pickup(m, cell, hero, stats["collected"])
            hero.turns += 1
        hero.turns += 1
        colour = DOOR_INDEX[m[dy][dx]]
        hero.keys[colour] -= 1
        m[dy][dx] = FLOOR
        stats["doors"] += 1
        pos = (dx, dy)


# ---------------------------------------------------------------------------
# structural analysis (enemies ignored, used for key reachability / repair)


def _structural_closure(rows: list[str], start, keys0, forbid: int | None = None):
    """Key-door only reachability.

    Enemies are treated as passable so the result describes pure layout
    ordering.  Returns (reachable cells, {key char: [cells]}).  When *forbid*
    is a colour index, doors of that colour are never opened, which is exactly
    the test for "this key can only be reached through its own door".
    """
    m = [list(r) for r in rows]
    h, w = len(m), len(m[0])
    for y in range(h):
        for x in range(w):
            if m[y][x] in ENEMY:
                m[y][x] = FLOOR
    keys = list(keys0)
    key_cells: dict[str, list[tuple[int, int]]] = {}
    reach: set[tuple[int, int]] = set()
    while True:
        blocked = {(x, y) for y in range(h) for x in range(w)
                   if m[y][x] in DOOR}
        dist, _parent = _flood(m, start, blocked)
        reach = set(dist)
        changed = False
        for x, y in sorted(reach):
            ch = m[y][x]
            if ch in KEY:
                keys[KEY_INDEX[ch]] += 1
                key_cells.setdefault(ch, []).append((x, y))
                m[y][x] = FLOOR
                changed = True
        candidates = []
        for x, y in reach:
            for dx, dy in NEIGHBOURS:
                nx, ny = x + dx, y + dy
                if not (0 <= nx < w and 0 <= ny < h):
                    continue
                ch = m[ny][nx]
                if ch not in DOOR:
                    continue
                colour = DOOR_INDEX[ch]
                if colour == forbid or keys[colour] <= 0:
                    continue
                candidates.append((ny, nx, colour))
        if candidates:
            selected_y, nx, colour = min(candidates)
            keys[colour] -= 1
            m[selected_y][nx] = FLOOR
            changed = True
        if not changed:
            return reach, key_cells


def self_dependent_keys(rows: list[str], start, keys0):
    """Keys that disappear from reach when doors of their own colour are shut."""
    full_reach, key_cells = _structural_closure(rows, start, keys0)
    out: dict[int, list[tuple[int, int]]] = {}
    for colour in range(3):
        without, _cells = _structural_closure(rows, start, keys0, forbid=colour)
        for cell in key_cells.get(KEY[colour], []):
            if cell not in without:
                out.setdefault(colour, []).append(cell)
    unreachable = []
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in KEY and (x, y) not in key_cells.get(ch, []):
                unreachable.append((x, y, ch))
    return out, unreachable


def reach_without_colour(rows: list[str], start, keys0, colour: int):
    """Cells reachable when doors of *colour* are never opened."""
    return _structural_closure(rows, start, keys0, forbid=colour)[0]


# ---------------------------------------------------------------------------
# diagnosis for repair


def _relaxed_blocker(m: list[list[str]], sources, goal, hero: Hero, floor: int):
    """Cheapest route to the exit ignoring health/key shortages, then report
    the first barrier on it that is actually impossible right now."""
    w, h = _dims(m)
    dist = {}
    parent = {}
    heap = []
    tie = 0
    for cell in sources:
        dist[cell] = 0
        heapq.heappush(heap, (0, tie, cell))
        tie += 1
    while heap:
        d, _, cell = heapq.heappop(heap)
        if d != dist.get(cell):
            continue
        if cell == goal:
            break
        x, y = cell
        for dx, dy in NEIGHBOURS:
            nx, ny = x + dx, y + dy
            if not (0 <= nx < w and 0 <= ny < h) or m[ny][nx] == WALL:
                continue
            nxt = (nx, ny)
            cost = BARRIER_COST + 1 if m[ny][nx] in DOOR or m[ny][nx] in ENEMY else 1
            nd = d + cost
            if nd < dist.get(nxt, 1 << 60):
                dist[nxt] = nd
                parent[nxt] = cell
                heapq.heappush(heap, (nd, tie, nxt))
                tie += 1
    if goal not in dist:
        return "layout", None
    path = _reconstruct_from_sources(parent, goal)
    saved = hero.clone()
    for cell in path[1:]:
        x, y = cell
        ch = m[y][x]
        if ch in DOOR:
            colour = DOOR_INDEX[ch]
            if saved.keys[colour] <= 0:
                return "door", cell
            saved.keys[colour] -= 1
        elif ch in ENEMY:
            preview = fight_preview(saved, ch, floor)
            if preview is None:
                return "enemy", cell
            saved.hp -= preview[0]
    return "none", None


# ---------------------------------------------------------------------------
# floor / campaign solver


def analyze_floor(rows: list[str], floor: int, entry: Hero) -> FloorResult:
    """Solve one floor for *entry* state and return the resulting hero."""
    m = [list(r) for r in rows]
    h, w = len(m), len(m[0])
    stats: Counter = Counter()
    stats["collected"] = Counter()
    hero = entry.clone()
    start = _find_char(rows, "D")
    goal_char = GOAL if floor == FLOORS - 1 else "U"
    goal = _find_char(rows, goal_char)

    def finish(solvable: bool, reached: bool, reason: str = "",
               blocker: tuple | None = None) -> FloorResult:
        self_dependent = {}
        unreachable = []
        if start is not None:
            self_dependent, unreachable = self_dependent_keys(
                rows, start, entry.keys)
        return FloorResult(
            floor=floor,
            solvable=solvable,
            reached=reached,
            entry=entry.clone(),
            hero=hero,
            reason=reason,
            blocker=blocker,
            damage=stats["damage"],
            fights=stats["fights"],
            turns=hero.turns - entry.turns,
            collected=stats["collected"],
            self_dependent=self_dependent,
            unreachable_keys=unreachable,
            enemies_total=sum(1 for row in rows for ch in row if ch in ENEMY),
            walkable=sum(1 for row in rows for ch in row if ch != WALL),
            objects=sum(1 for row in rows for ch in row
                        if ch not in (WALL, FLOOR)),
        )

    if start is None or goal is None:
        return finish(False, False, "missing_stair")

    pos = start
    while True:
        pos, dist, _parent = _sweep(m, pos, hero, stats["collected"])
        if goal in dist:
            break
        path = _barrier_path(m, set(dist), goal, hero, floor)
        if path is None:
            reason, cell = _relaxed_blocker(m, set(dist), goal, hero, floor)
            blocker = (reason, cell) if cell is not None else None
            return finish(False, False, reason, blocker)
        # Walking to the start of the barrier route is free movement inside
        # the already reachable region.
        hero.turns += dist.get(path[0], 0)
        outcome, cell = _execute_to_barrier(m, path, hero, floor, stats)
        if outcome == "blocked":
            reason, why = _relaxed_blocker(m, set(dist), goal, hero, floor)
            blocker = (reason, why) if why is not None else None
            return finish(False, False, reason or "blocked", blocker)
        if outcome == "exit":
            break
        pos = cell

    if goal not in _flood(m, pos, _blocked_cells(m))[0]:
        return finish(False, False, "exit_lost")

    pos = _clear_optional_doors(m, pos, hero, stats)

    dist, _parent = _flood(m, pos, _blocked_cells(m))
    if goal not in dist:
        return finish(False, False, "exit_lost")
    hero.turns += dist[goal]
    return finish(True, True)


def solve_campaign(floors: list[list[str]], verbose: bool = False) -> CampaignResult:
    hero = Hero()
    results: list[FloorResult] = []
    failed = None
    for index, rows in enumerate(floors):
        result = analyze_floor(rows, index, hero)
        results.append(result)
        if verbose:
            print(f"  {index + 1:2d}F {len(rows[0])}x{len(rows)} "
                  f"hp={result.hero.hp:5d} atk={result.hero.attack:3d} "
                  f"def={result.hero.defence:3d} "
                  f"dmg={result.damage:5d} fights={result.fights:2d} "
                  f"{'OK' if result.solvable else 'FAIL ' + result.reason}")
        if not result.solvable:
            failed = index
            break
        hero = result.hero

    collected = Counter()
    for result in results:
        collected.update(result.collected)
    max_hp = START_HP + POTION_HEAL * collected["potion"] + \
        BIG_POTION_HEAL * collected["big_potion"]

    regions = []
    running_max = START_HP
    for region in range(5):
        chunk = results[region * REGION:(region + 1) * REGION]
        if not chunk:
            break
        for result in chunk:
            running_max += (POTION_HEAL * result.collected["potion"] +
                            BIG_POTION_HEAL * result.collected["big_potion"])
        regions.append(RegionSummary(
            region=region,
            floors=len(chunk),
            enemies=sum(result.enemies_total for result in chunk),
            avg_density=sum(result.density for result in chunk
                            if result.floor % REGION != REGION - 1) /
            max(1, sum(1 for result in chunk
                       if result.floor % REGION != REGION - 1)),
            boss_density=chunk[-1].density if chunk else 0.0,
            final_hp=chunk[-1].hero.hp,
            max_hp=running_max,
        ))

    return CampaignResult(
        completed=failed is None,
        floors=results,
        hero=hero,
        max_hp=max_hp,
        regions=regions,
        failed_floor=failed,
    )


# ---------------------------------------------------------------------------
# CLI


def _load_generated() -> list[list[str]]:
    from gen_tower import read_generated_c
    root = Path(__file__).resolve().parents[1]
    return read_generated_c(root / "main" / "tower_floors.c")


def main() -> None:
    floors = _load_generated()
    print(f"loaded {len(floors)} floors from main/tower_floors.c")
    campaign = solve_campaign(floors, verbose=True)
    print()
    print("region  floors  enemies  nonboss density  boss density  final HP  "
          "max HP  margin")
    for region in campaign.regions:
        print(f"  {region.region + 1:^4d}  {region.floors:^6d}  "
              f"{region.enemies:^7d}  {region.avg_density:^15.1%}  "
              f"{region.boss_density:^12.1%}  {region.final_hp:^8d}  "
              f"{region.max_hp:^6d}  {region.hp_fraction:^6.1%}")
    print()
    if campaign.completed:
        print(f"CAMPAIGN COMPLETE: hp={campaign.hero.hp}/{campaign.max_hp} "
              f"({campaign.margin:.1%}) atk={campaign.hero.attack} "
              f"def={campaign.hero.defence} gold={campaign.hero.gold} "
              f"turns={campaign.hero.turns} keys={campaign.hero.keys}")
    else:
        print(f"CAMPAIGN FAILED on floor {campaign.failed_floor + 1}")
    locked = [(r.floor + 1, r.self_dependent) for r in campaign.floors
              if r.self_dependent]
    print(f"self-dependent keys: {locked if locked else 'none'}")


if __name__ == "__main__":
    main()
