"""Replay designer floors through an immutable snapshot of the real C model.

No Python combat model or production-map writes. Floor numbers in ``state`` are
zero based; ``floor(number)`` uses the human-facing one-based floor number.
"""
import ctypes as C
from collections import deque
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('floor x y hp attack defence yellow blue red gold turns phase cleared '
         'chapters damage winnable status shop_attack shop_defence explored_lo '
         'explored_hi pending_x pending_y pending_tile pending_hp pending_attack '
         'pending_defence pending_gold pending_rounds').split()
ENEMY_NAMES = 'tile hp attack defence gold damage rounds winnable'.split()


def _rows(floor):
    return list(floor['rows'] if isinstance(floor, dict) else floor.rows)


def build(floors, out, model_transform=None, headers=None):
    """Compile actual tower.c with supplied rows; pad only missing floors.

    ``model_transform(source)`` can supply exact designer-owned hooks. Original
    source hashes and the compiled-source hash are recorded in provenance.
    ``headers`` adds relative include files without replacing model snapshots.
    Each output directory should belong to one build (loaded DLLs stay locked).
    """
    floors, out = list(floors), Path(out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    model = out / 'model'
    model.mkdir(exist_ok=True)
    paths = [ROOT / 'main' / n for n in ('tower.c', 'tower.h', 'tower_floors.h')]
    data = {p.name: p.read_bytes() for p in paths}
    assert all(p.read_bytes() == data[p.name] for p in paths), 'Model changed while snapshotting; rerun'
    count = int(re.search(rb'#define\s+TOWER_FLOORS\s+(\d+)', data['tower.h'])[1])
    if not 1 <= len(floors) <= count:
        raise ValueError(f'Expected 1..{count} floors, received {len(floors)}')
    for name, blob in data.items():
        (model / name).write_bytes(blob)
    source = data['tower.c'].decode('utf-8')
    if model_transform is not None:
        source = model_transform(source)
        if not isinstance(source, str):
            raise TypeError('model_transform must return C source text')
        (model / 'tower.c').write_text(source, encoding='utf-8')
    for name, content in (headers or {}).items():
        target = (model / name).resolve()
        if not target.is_relative_to(model.resolve()) or target == model.resolve():
            raise ValueError(f'Header escapes model directory: {name}')
        if target.name in data or target.name in ('floors.c', 'bridge.c'):
            raise ValueError(f'Header replaces reserved model file: {name}')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding='utf-8')
    generated, entries = ['#include "tower_floors.h"'], []
    for index, floor in enumerate(floors):
        rows = _rows(floor)
        if not rows or not rows[0] or len(rows) > 255 or len(rows[0]) > 255:
            raise ValueError(f'Invalid dimensions for floor {index + 1}')
        if any(len(r) != len(rows[0]) or not r.isascii() or '\x00' in r for r in rows):
            raise ValueError(f'Nonrectangular or non-ASCII floor {index + 1}')
        generated.append(f'static const char *const f{index}[]={{' + ','.join(json.dumps(r) for r in rows) + '};')
        entries.append(f'{{{len(rows[0])},{len(rows)},f{index}}}')
    if len(floors) < count:
        generated.append('/* Explicit inert fixtures, not designed campaign floors. */')
        generated.append('static const char *const fixture[]={"#####","#D.U#","#####"};')
        entries.extend(['{5,3,fixture}'] * (count - len(floors)))
    generated.append('const tower_floor_desc_t tower_floor_table[TOWER_FLOORS]={' + ','.join(entries) + '};')
    (model / 'floors.c').write_text('\n'.join(generated), encoding='utf-8')
    (model / 'bridge.c').write_text(r'''#include "tower.h"
#include "tower_floors.h"
#include <stdlib.h>
#include <string.h>
void lab_state(tower_handle_t h, unsigned *o) {
 tower_snapshot_t s; tower_read(h,&s);
 unsigned v[]={s.floor,s.x,s.y,s.hp,s.attack,s.defence,s.keys[0],s.keys[1],s.keys[2],
 s.gold,s.turns,s.phase,s.cleared,s.chapters,s.pending.damage,s.pending.winnable,s.status,
 s.shop_attack,s.shop_defence,s.explored_lo,s.explored_hi,s.pending_x,s.pending_y,
 s.pending.tile,s.pending.hp,s.pending.attack,s.pending.defence,s.pending.gold,s.pending.rounds};
 memcpy(o,v,sizeof(v));
}
int lab_roundtrip(tower_handle_t h) {
 unsigned char *a=malloc(TOWER_SAVE_BYTES),*b=malloc(TOWER_SAVE_BYTES);
 tower_handle_t t=NULL;
 if(!a||!b||tower_create(NULL,&t)!=TOWER_OK) {free(a);free(b);return -9;}
 int r=tower_export(h,a,TOWER_SAVE_BYTES);
 if(!r) r=tower_import(t,a,TOWER_SAVE_BYTES);
 if(!r) r=tower_export(t,b,TOWER_SAVE_BYTES);
 if(!r && memcmp(a,b,TOWER_SAVE_BYTES)) r=-8;
 tower_delete(t);free(a);free(b);return r;
}
int lab_walkable(tower_handle_t h,int x,int y) {
 tower_tile_t t=tower_tile_current(h,x,y);
 return t!=TOWER_TILE_WALL && !(t>=TOWER_TILE_DOOR_YELLOW && t<=TOWER_TILE_DOOR_RED)
 && !(t>=TOWER_TILE_SLIME && t<=TOWER_TILE_DEMON)
 && t!=TOWER_TILE_UP && t!=TOWER_TILE_DOWN && t!=TOWER_TILE_GOAL
 && t!=TOWER_TILE_SECRET && t!=TOWER_TILE_EVENT;
}
int lab_is_knight(tower_handle_t h,int x,int y) {return tower_tile_current(h,x,y)==TOWER_TILE_KNIGHT;}
int lab_enemy(tower_handle_t h,int tile,unsigned *o) {
 tower_enemy_preview_t p;
 int r=tower_enemy_preview(h,(tower_tile_t)tile,&p);
 if(r) return r;
 unsigned v[]={p.tile,p.hp,p.attack,p.defence,p.gold,p.damage,p.rounds,p.winnable};
 memcpy(o,v,sizeof(v));return r;
}
unsigned lab_width(unsigned floor) {return floor<TOWER_FLOORS?tower_floor_table[floor].w:0;}
unsigned lab_height(unsigned floor) {return floor<TOWER_FLOORS?tower_floor_table[floor].h:0;}
''', encoding='utf-8')
    provenance = {n: hashlib.sha256(b).hexdigest() for n, b in data.items()}
    provenance.update(compiled_source_sha256=hashlib.sha256((model / 'tower.c').read_bytes()).hexdigest(),
                      designed_floors=len(floors), fixture_floors=count-len(floors),
                      transformed=model_transform is not None,
                      extra_headers={n: hashlib.sha256(v.encode()).hexdigest() for n, v in (headers or {}).items()})
    (out / 'model-provenance.json').write_text(json.dumps(provenance, indent=2), encoding='utf-8')
    cc = os.environ.get('CC') or shutil.which('gcc') or r'D:\espbulid\mingw64\bin\gcc.exe'
    env = os.environ.copy()
    env['PATH'] = str(Path(cc).parent) + os.pathsep + env.get('PATH', '')
    env['TMP'] = env['TEMP'] = str(out)
    dll = out / ('tower_design.dll' if os.name == 'nt' else 'tower_design.so')
    cmd = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2', '-shared']
    if os.name != 'nt':
        cmd += ['-fPIC']
    cmd += ['-I', str(model), str(model/'tower.c'), str(model/'floors.c'), str(model/'bridge.c'), '-o', str(dll)]
    result = subprocess.run(cmd, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(result.stderr)
    dll_directory = os.add_dll_directory(str(Path(cc).parent)) if os.name == 'nt' else None
    lib = C.CDLL(str(dll))
    lib._lab_dll_directory = dll_directory
    lib.tower_create.argtypes = [C.c_void_p, C.POINTER(C.c_void_p)]
    lib.tower_delete.argtypes = [C.c_void_p]
    lib.tower_delete.restype = None
    for name in ('tower_confirm', 'tower_cancel', 'lab_roundtrip'):
        getattr(lib, name).argtypes = [C.c_void_p]
    for name in ('tower_move', 'tower_tile_current', 'lab_walkable', 'lab_is_knight'):
        getattr(lib, name).argtypes = [C.c_void_p, C.c_int, C.c_int]
    lib.tower_buy.argtypes = [C.c_void_p, C.c_int]
    lib.tower_teleport.argtypes = [C.c_void_p, C.c_uint]
    lib.tower_anchor_floor.argtypes = [C.c_uint]
    lib.lab_state.argtypes = [C.c_void_p, C.POINTER(C.c_uint)]
    lib.lab_state.restype = None
    lib.lab_enemy.argtypes = [C.c_void_p, C.c_int, C.POINTER(C.c_uint)]
    for name in ('lab_width', 'lab_height'):
        getattr(lib, name).argtypes = [C.c_uint]
        getattr(lib, name).restype = C.c_uint
    return lib


class Replay:
    def __init__(self, lib, floors, out, name):
        self.lib, self.floors, self.out, self.name = lib, list(floors), Path(out), name
        self.handle = C.c_void_p()
        assert lib.tower_create(None, C.byref(self.handle)) == 0, 'tower_create failed'
        self.moves, self.checkpoints = [], []
        assert lib.lab_roundtrip(self.handle) == 0, 'initial save roundtrip failed'

    def state(self):
        values = (C.c_uint * len(NAMES))()
        self.lib.lab_state(self.handle, values)
        return dict(zip(NAMES, values))

    def tile(self, x, y):
        return self.lib.tower_tile_current(self.handle, x, y)

    def record(self, action, before, **details):
        after = self.state()
        assert self.lib.lab_roundtrip(self.handle) == 0, ('save roundtrip', self.name, action, after)
        self.moves.append(dict(action=action, before=before, after=after, **details))
        return after

    def checkpoint(self, label=None):
        state = self.state()
        self.checkpoints.append(state if label is None else dict(name=label, state=state))
        return state

    def preview(self, tile):
        """Read actual C enemy preview without moving or confirming combat."""
        before, values = self.state(), (C.c_uint * len(ENEMY_NAMES))()
        result = self.lib.lab_enemy(self.handle, tile, values)
        preview = dict(zip(ENEMY_NAMES, values)) if result == 0 else None
        self.record(['preview', tile], before, result=result, preview=preview)
        assert result == 0, ('not an enemy tile', tile)
        return preview

    def step(self, dx, dy, allow_death=False):
        before = self.state()
        result = self.lib.tower_move(self.handle, dx, dy)
        after = self.record(['move', dx, dy], before, result=result)
        if after['phase'] == 1:
            assert allow_death or after['winnable'], ('lethal planned fight', self.name, before, after)
            result = self.lib.tower_confirm(self.handle)
            after = self.record(['confirm'], after, result=result)
        assert after['hp'] or allow_death, ('unexpected death', self.name, after)
        return after

    def go(self, x, y):
        initial = self.state()
        floor = initial['floor']
        width, height = self.lib.lab_width(floor), self.lib.lab_height(floor)
        target = (x, y)
        assert 0 <= x < width and 0 <= y < height, ('waypoint outside floor', self.name, floor, target, width, height)
        # A shortest path is at most area-1 cells; opening an explicit secret
        # can require one extra contact. Replanning remains finitely bounded.
        for _ in range(3 * width * height + 1):
            state = self.state()
            start = (state['x'], state['y'])
            if start == target:
                return state
            queue, parent = deque([start]), {start: None}
            while queue and target not in parent:
                cx, cy = queue.popleft()
                for point in ((cx+1, cy), (cx-1, cy), (cx, cy+1), (cx, cy-1)):
                    if point in parent or not (0 <= point[0] < width and 0 <= point[1] < height):
                        continue
                    tile = self.tile(*point)
                    if tile == 1 or (point != target and not self.lib.lab_walkable(self.handle, *point)):
                        continue
                    parent[point] = (cx, cy)
                    queue.append(point)
            assert target in parent, ('blocked waypoint', self.name, state, target, dict(width=width, height=height, reachable=len(parent), target_tile=self.tile(x, y)))
            point = target
            while parent[point] != start:
                point = parent[point]
            before_tile = self.tile(*point)
            after = self.step(point[0]-start[0], point[1]-start[1])
            if after['floor'] != floor:
                assert point == target, ('unexpected floor transition', self.name, target, after)
                return after
            assert (after['x'], after['y']) != start or self.tile(*point) != before_tile, ('waypoint made no progress', self.name, state, target, point, before_tile, after)
        raise AssertionError(('waypoint exceeded area bound', self.name, target, self.state(), width, height))

    def stair(self, up):
        before = self.state()
        floor, tile = before['floor'], 22 if up else 23
        width, height = self.lib.lab_width(floor), self.lib.lab_height(floor)
        stairs = [(x, y) for y in range(height) for x in range(width) if self.tile(x, y) == tile]
        assert len(stairs) == 1, ('expected one stair', self.name, floor, tile, stairs)
        x, y = stairs[0]
        if (before['x'], before['y']) == (x, y):
            neighbors = [p for p in ((x+1,y), (x-1,y), (x,y+1), (x,y-1))
                         if 0 <= p[0] < width and 0 <= p[1] < height and self.lib.lab_walkable(self.handle, *p)]
            assert neighbors, ('no safe step off stair', self.name, before)
            self.go(*neighbors[0])
        self.go(x, y)
        assert self.state()['floor'] == floor + (1 if up else -1), ('stair failed', self.name, before, self.state())
        return self.checkpoint()

    def floor(self, number):
        assert 1 <= number <= self.lib.tower_floors(), ('invalid floor', number)
        while self.state()['floor'] != number - 1:
            self.stair(self.state()['floor'] < number - 1)
        return self.state()

    def buy(self, kind):
        before = self.state()
        assert self.tile(before['x'], before['y']) == 20, ('purchase requires standing on shop', self.name, before)
        result = self.lib.tower_buy(self.handle, kind)
        after = self.record(['buy', kind], before, result=result)
        assert after['gold'] < before['gold'], ('purchase failed', self.name, kind, before, after)
        return after

    def warp(self, anchor):
        """Teleport using the C API's zero-based anchor index (0..5)."""
        assert isinstance(anchor, int) and 0 <= anchor < 6, ('invalid anchor', anchor)
        before = self.state()
        result = self.lib.tower_teleport(self.handle, anchor)
        return self.record(['warp', anchor], before, result=result)

    def close(self):
        if self.handle:
            final = self.state()
            self.out.mkdir(parents=True, exist_ok=True)
            (self.out / f'{self.name}.json').write_text(json.dumps(dict(name=self.name, final=final, checkpoints=self.checkpoints, actions=self.moves), indent=2), encoding='utf-8')
            self.lib.tower_delete(self.handle)
            self.handle = C.c_void_p()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
