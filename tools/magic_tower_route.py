#!/usr/bin/env python3
"""Plan a complete ten-floor route from the real map data and bake both the
JSON route used by the host integration test and the C header used by the
portable model test."""
import json
import re
import sys
from collections import deque
from pathlib import Path

W = H = 11
PROJECT = Path(__file__).resolve().parents[1]
MOVE = {'left': (-1, 0), 'right': (1, 0), 'up': (0, -1), 'down': (0, 1)}
ACTION_CODE = {'left': 0, 'right': 1, 'up': 2, 'down': 5, 'confirm': 6}

ENEMIES = {
    's': (24, 6, 2, 3, 'Slime'),
    'v': (35, 11, 4, 6, 'Bat'),
    'k': (70, 20, 8, 12, 'Knight'),
    'X': (999, 200, 120, 0, 'Sentinel'),
    'm': (120, 22, 10, 18, 'Mage'),
    'g': (180, 30, 16, 26, 'Golem'),
    'w': (260, 36, 22, 34, 'Wraith'),
    'M': (700, 58, 46, 150, 'Demon King'),
}
ITEMS = {'p': ('hp', 30), 'P': ('hp', 80), 'a': ('atk', 5), 'd': ('def', 3)}
KEYS = {'y': 0, 'b': 1, 'r': 2}
DOORS = {'Y': 0, 'B': 1, 'R': 2}
LOOT = set(ITEMS) | set(KEYS) | {'n'}


def load_maps():
    text = (PROJECT / 'main' / 'tower.c').read_text(encoding='utf-8')
    block = text.split('initial_map[TOWER_FLOORS]')[1].split('};')[0]
    rows = re.findall(r'"([^"]*)"', block)
    assert len(rows) == 10 * H, len(rows)
    assert all(len(row) == W for row in rows)
    return [[list(row) for row in rows[f * H:(f + 1) * H]] for f in range(10)]


class Route:
    def __init__(self, maps):
        self.maps = maps
        self.floor, self.x, self.y = 0, 5, 9
        self.hp, self.atk, self.dfn = 100, 12, 4
        self.keys = [0, 0, 0]
        self.gold = self.turns = 0
        self.cleared = False
        self.actions = []
        self.encounters = []
        self.npc_seen = set()

    def tile(self, f=None, x=None, y=None):
        return self.maps[self.floor if f is None else f][
            self.y if y is None else y][self.x if x is None else x]

    def block_reason(self, t, x=None, y=None):
        if t == '#':
            return 'wall'
        if t == 'S':
            return 'shop'
        if t == 'n' and x is not None and (self.floor, x, y) in self.npc_seen:
            return 'visited npc'
        if t in DOORS and self.keys[DOORS[t]] == 0:
            return 'locked'
        if t in ENEMIES:
            ehp, eatk, edef, _, name = ENEMIES[t]
            if self.atk <= edef:
                return 'too strong'
        return None

    def bfs(self, goals, allow_enemy=False):
        """Shortest path to any goal cell; returns cells excluding the start."""
        start = (self.x, self.y)
        if start in goals:
            return []
        parent = {start: None}
        queue = deque([start])
        while queue:
            cx, cy = queue.popleft()
            for dx, dy in MOVE.values():
                nx, ny = cx + dx, cy + dy
                if not (0 <= nx < W and 0 <= ny < H):
                    continue
                if (nx, ny) in parent:
                    continue
                t = self.maps[self.floor][ny][nx]
                if t == '#':
                    continue
                blocked = self.block_reason(t, nx, ny)
                if blocked == 'shop':
                    continue
                if blocked == 'locked':
                    continue
                if blocked == 'too strong':
                    continue
                if t in ENEMIES and not allow_enemy and (nx, ny) not in goals:
                    continue
                parent[(nx, ny)] = (cx, cy)
                if (nx, ny) in goals:
                    path = []
                    node = (nx, ny)
                    while parent[node] is not None:
                        path.append(node)
                        node = parent[node]
                    return path[::-1]
                queue.append((nx, ny))
        return None

    def step(self, direction):
        dx, dy = MOVE[direction]
        nx, ny = self.x + dx, self.y + dy
        t = self.maps[self.floor][ny][nx]
        assert t != '#'
        self.actions.append(direction)
        if t in ENEMIES:
            ehp, eatk, edef, egold, name = ENEMIES[t]
            assert self.atk > edef, (self.floor + 1, name, self.atk, edef)
            hit = self.atk - edef
            rounds = (ehp + hit - 1) // hit
            dmg = (rounds - 1) * max(0, eatk - self.dfn)
            assert dmg < self.hp, (self.floor + 1, name, dmg, self.hp)
            self.hp -= dmg
            self.gold += egold
            self.maps[self.floor][ny][nx] = '.'
            self.actions.append('confirm')
            self.encounters.append({'floor': self.floor, 'x': nx, 'y': ny,
                                    'name': name, 'damage': dmg})
        elif t in ITEMS:
            kind, value = ITEMS[t]
            if kind == 'hp':
                self.hp += value
            elif kind == 'atk':
                self.atk += value
            else:
                self.dfn += value
            self.maps[self.floor][ny][nx] = '.'
        elif t in KEYS:
            self.keys[KEYS[t]] += 1
            self.maps[self.floor][ny][nx] = '.'
        elif t in DOORS:
            color = DOORS[t]
            assert self.keys[color] > 0, (self.floor + 1, t, self.keys)
            self.keys[color] -= 1
            self.maps[self.floor][ny][nx] = '.'
        elif t == 'n':
            self.npc_seen.add((self.floor, nx, ny))
            self.actions.append('confirm')
        self.x, self.y = nx, ny
        if t == 'U':
            assert self.floor + 1 < len(self.maps)
            self.floor += 1
            self.x, self.y = 5, 9
        elif t == 'D':
            assert self.floor > 0
            self.floor -= 1
            self.x, self.y = 5, 1
        elif t == 'G':
            self.cleared = True
        self.turns += 1

    def walk(self, path, allow_enemy=False):
        for nx, ny in path:
            direction = next(k for k, v in MOVE.items()
                             if (self.x + v[0], self.y + v[1]) == (nx, ny))
            self.step(direction)
            if (self.x, self.y) != (nx, ny):
                return False
        return True

    def loot_goals(self):
        return {(x, y) for y in range(H) for x in range(W)
                if self.maps[self.floor][y][x] in LOOT and
                (self.floor, x, y) not in self.npc_seen}

    def enemy_goals(self):
        return {(x, y) for y in range(H) for x in range(W)
                if self.maps[self.floor][y][x] in ENEMIES}

    def enemy_damage(self, t):
        ehp, eatk, edef, _, _ = ENEMIES[t]
        hit = self.atk - edef
        rounds = (ehp + hit - 1) // hit
        return (rounds - 1) * max(0, eatk - self.dfn)

    def collect_floor(self, verbose=True):
        """Clear every reachable reward and fight gates until the up stairs
        are reachable. Returns 'up', 'goal', or 'stuck'."""
        while True:
            loot = self.loot_goals()
            path = self.bfs(loot) if loot else None
            if path:
                target = path[-1]
                if verbose:
                    print(f"  F{self.floor + 1} loot {self.maps[self.floor][target[1]][target[0]]}"
                          f" at {target} from ({self.x},{self.y})")
                self.walk(path)
                continue
            up = self.bfs({(5, 1)})
            if up:
                if self.maps[self.floor][1][5] == 'U':
                    if verbose:
                        print(f"  F{self.floor + 1} stairs at {(5, 1)}")
                    self.walk(up)
                    return 'up'
                if self.maps[self.floor][1][5] == 'G':
                    self.walk(up)
                    return 'goal' if self.cleared else 'stuck'
            enemies = self.enemy_goals()
            path = self.bfs(enemies, allow_enemy=True) if enemies else None
            if path:
                target = path[-1]
                t = self.maps[self.floor][target[1]][target[0]]
                if self.atk <= ENEMIES[t][2]:
                    return 'stuck'
                if verbose:
                    print(f"  F{self.floor + 1} fight {ENEMIES[t][4]} at {target} "
                          f"(dmg {self.enemy_damage(t)}, hp {self.hp})")
                self.walk(path, allow_enemy=True)
                continue
            return 'stuck'

    def descend(self):
        # The player spawns ON the down stair after entering a floor.
        # Move to an adjacent tile and step back onto D to descend.
        neighbours = {(5 + dx, 9 + dy) for dx, dy in MOVE.values()
                      if 0 <= 5 + dx < W and 0 <= 9 + dy < H
                      and self.maps[self.floor][9 + dy][5 + dx] != '#'}
        path = self.bfs(neighbours)
        if path is None:
            return False
        previous_floor = self.floor
        self.walk(path)
        if self.floor != previous_floor:
            return True
        direction = next((name for name, (dx, dy) in MOVE.items()
                          if (self.x + dx, self.y + dy) == (5, 9)), None)
        if direction is None:
            return False
        self.step(direction)
        return self.floor < previous_floor

    def summary(self):
        return {'floor': self.floor, 'x': self.x, 'y': self.y,
                'hp': self.hp, 'attack': self.atk, 'defence': self.dfn,
                'keys': self.keys, 'gold': self.gold, 'turns': self.turns,
                'cleared': self.cleared}


def main():
    maps = load_maps()
    route = Route(maps)
    stale = 0
    while not route.cleared:
        print(f"--- F{route.floor + 1} ({route.x},{route.y}) HP{route.hp} "
              f"ATK{route.atk} DEF{route.dfn} KEY{route.keys}")
        result = route.collect_floor()
        if result == 'goal':
            break
        if result == 'up':
            stale = 0
            continue
        # Blocked: look below for the missing key or supplies. Keep descending
        # through floors whose rewards were already collected so a detour can
        # reach the third floor from the fifth without bouncing off the fourth.
        if route.floor == 0:
            raise AssertionError('stuck on the first floor')
        print(f"  F{route.floor + 1} blocked; descending")
        while route.floor > 0:
            if not route.descend():
                raise AssertionError(f'cannot descend from F{route.floor + 1}')
            # Do not stop at loot hidden behind an unavailable key.
            # A reachable guard may be the only path to the next key.
            if (route.bfs(route.loot_goals()) is not None or
                    route.bfs(route.enemy_goals(), allow_enemy=True) is not None):
                break
        stale += 1
        if stale > 20:
            raise AssertionError('route is cycling')

    print('FINAL', route.summary())
    assert route.cleared
    assert route.floor == len(maps) - 1
    payload = {
        'description': 'Generated by tools/magic_tower_route.py: ten-floor ascent with the third/fourth-floor key detour.',
        'actions': route.actions,
        'expected': route.summary(),
        'enemy_encounters': route.encounters,
    }
    json_path = PROJECT / 'tests' / 'win_route.json'
    json_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    header = ['/* Generated by tools/magic_tower_route.py. Do not edit. */',
              '#ifndef MAGIC_TOWER_WIN_ROUTE_H', '#define MAGIC_TOWER_WIN_ROUTE_H', '',
              'static const signed char WIN_ROUTE[] = {']
    header.append('    ' + ','.join(str(ACTION_CODE[a]) for a in route.actions))
    header += ['};', '', '#define WIN_ROUTE_COUNT (sizeof(WIN_ROUTE)/sizeof(WIN_ROUTE[0]))']
    final = route.summary()
    header += [f'#define WIN_EXPECT_FLOOR {final["floor"]}',
               f'#define WIN_EXPECT_X {final["x"]}',
               f'#define WIN_EXPECT_Y {final["y"]}',
               f'#define WIN_EXPECT_HP {final["hp"]}',
               f'#define WIN_EXPECT_ATTACK {final["attack"]}',
               f'#define WIN_EXPECT_DEFENCE {final["defence"]}',
               f'#define WIN_EXPECT_GOLD {final["gold"]}',
               f'#define WIN_EXPECT_TURNS {final["turns"]}',
               '#endif', '']
    (PROJECT / 'tests' / 'win_route.h').write_text('\n'.join(header), encoding='utf-8')
    print('actions', len(route.actions), 'wrote', json_path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
