"""Designer lab: replay coordinate plans through an immutable copy of real tower.c.

Never writes production maps. 11-50 are explicit inert test fixtures, not designed
or validated campaign floors. C owns combat, items, movement and save validation.
"""
import argparse
import ctypes as C
from collections import deque
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/region1-design'
OUT.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('region1_blueprint', ROOT/'assets_src/level_design/region1_blueprint.py')
plans = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plans)
FLOORS = plans.FLOORS
NAMES = 'floor x y hp attack defence yellow blue red gold turns phase cleared chapters damage winnable status'.split()

def build():
    model = OUT/'model'
    model.mkdir(exist_ok=True)
    paths = [ROOT/'main'/n for n in ('tower.c','tower.h','tower_floors.h')]
    data = {p.name:p.read_bytes() for p in paths}
    assert all(p.read_bytes()==data[p.name] for p in paths), 'Model changed while snapshotting; rerun'
    for name,blob in data.items():
        (model/name).write_bytes(blob)
    (OUT/'model-provenance.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in data.items()},indent=2))
    generated = ['#include "tower_floors.h"']
    for f in FLOORS:
        generated.append(f'static const char *const f{f.number}[]={{'+','.join(json.dumps(r) for r in f.rows)+'};')
    generated.append('static const char *const fixture[]={"#####","#D.U#","#####"};')
    entries = [f'{{11,11,f{f.number}}}' for f in FLOORS]+['{5,3,fixture}']*40
    generated.append('const tower_floor_desc_t tower_floor_table[TOWER_FLOORS]={'+','.join(entries)+'};')
    (model/'floors.c').write_text('\n'.join(generated))
    (model/'bridge.c').write_text('''#include "tower.h"
#include <stdlib.h>
#include <string.h>
void lab_state(tower_handle_t h, unsigned *o) {
 tower_snapshot_t s; tower_read(h,&s);
 unsigned v[]={s.floor,s.x,s.y,s.hp,s.attack,s.defence,s.keys[0],s.keys[1],s.keys[2],
 s.gold,s.turns,s.phase,s.cleared,s.chapters,s.pending.damage,s.pending.winnable,s.status};
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
 && t!=TOWER_TILE_UP && t!=TOWER_TILE_DOWN && t!=TOWER_TILE_GOAL;
}
int lab_is_knight(tower_handle_t h,int x,int y) {return tower_tile_current(h,x,y)==TOWER_TILE_KNIGHT;}
''')
    cc=os.environ.get('CC') or shutil.which('gcc') or r'D:\espbulid\mingw64\bin\gcc.exe'
    env=os.environ.copy()
    env['PATH']=str(Path(cc).parent)+os.pathsep+env.get('PATH','')
    env['TMP']=env['TEMP']=str(OUT.resolve())
    dll=OUT/('tower_design.dll' if os.name=='nt' else 'tower_design.so')
    cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2','-shared']
    if os.name!='nt': cmd+=['-fPIC']
    cmd+=['-I',str(model),str(model/'tower.c'),str(model/'floors.c'),str(model/'bridge.c'),'-o',str(dll)]
    result=subprocess.run(cmd,env=env,capture_output=True,text=True)
    (OUT/'compile.log').write_text(result.stdout+result.stderr)
    if result.returncode: raise RuntimeError(result.stderr)
    if os.name=='nt': os.add_dll_directory(str(Path(cc).parent))
    lib=C.CDLL(str(dll))
    lib.tower_create.argtypes=[C.c_void_p,C.POINTER(C.c_void_p)]
    for name in ('tower_delete','tower_confirm','lab_roundtrip'):
        getattr(lib,name).argtypes=[C.c_void_p]
    lib.tower_move.argtypes=[C.c_void_p,C.c_int,C.c_int]
    lib.tower_tile_current.argtypes=[C.c_void_p,C.c_int,C.c_int]
    lib.tower_buy.argtypes=[C.c_void_p,C.c_int]
    lib.lab_walkable.argtypes=[C.c_void_p,C.c_int,C.c_int]
    lib.lab_is_knight.argtypes=[C.c_void_p,C.c_int,C.c_int]
    lib.lab_state.argtypes=[C.c_void_p,C.POINTER(C.c_uint)]
    return lib

class Replay:
    def __init__(self,lib,name):
        self.lib,self.name=lib,name
        self.handle=C.c_void_p()
        assert lib.tower_create(None,C.byref(self.handle))==0
        self.moves=[]; self.checkpoints=[]
    def state(self):
        values=(C.c_uint*len(NAMES))()
        self.lib.lab_state(self.handle,values)
        return dict(zip(NAMES,values))
    def tile(self,x,y): return self.lib.tower_tile_current(self.handle,x,y)
    def record(self,action,before):
        after=self.state()
        assert self.lib.lab_roundtrip(self.handle)==0, ('save roundtrip',self.name,action,after)
        self.moves.append(dict(action=action,before=before,after=after))
    def step(self,dx,dy,allow_death=False):
        before=self.state()
        self.lib.tower_move(self.handle,dx,dy)
        after=self.state()
        self.record(['move',dx,dy],before)
        if after['phase']==1:
            if not allow_death:
                assert after['winnable'], ('lethal planned fight',self.name,before,after)
            self.lib.tower_confirm(self.handle)
            self.record(['confirm'],after)
        assert self.state()['hp'] or allow_death, ('unexpected death',self.name)
    def go(self,x,y):
        initial_floor=self.state()['floor']
        for _ in range(200):
            s=self.state(); start=(s['x'],s['y'])
            if start==(x,y): return
            q=deque([start]); parent={start:None}
            while q and (x,y) not in parent:
                cx,cy=q.popleft()
                for p in ((cx+1,cy),(cx-1,cy),(cx,cy+1),(cx,cy-1)):
                    if p in parent or not (0<=p[0]<11 and 0<=p[1]<11): continue
                    # Barriers/stairs are crossed only when explicitly targeted.
                    if FLOORS[initial_floor].rows[p[1]][p[0]]=='#':continue
                    if p!=(x,y) and not self.lib.lab_walkable(self.handle,*p):continue
                    parent[p]=(cx,cy);q.append(p)
            assert (x,y) in parent, ('blocked waypoint',self.name,s,(x,y))
            p=(x,y)
            while parent[p]!=start: p=parent[p]
            self.step(p[0]-start[0],p[1]-start[1])
            if self.state()['floor']!=initial_floor: return
        raise AssertionError('waypoint did not settle')
    def stair(self,up):
        s=self.state()
        symbol='U' if up else 'D'
        rows=FLOORS[s['floor']].rows
        x,y=next((x,y) for y,row in enumerate(rows) for x,c in enumerate(row) if c==symbol)
        if (s['x'],s['y'])==(x,y):
            p=next(p for p in ((x+1,y),(x-1,y),(x,y+1),(x,y-1))
                   if 0<=p[0]<11 and 0<=p[1]<11 and self.lib.lab_walkable(self.handle,*p))
            self.go(*p)
        self.go(x,y)
        assert self.state()['floor']==s['floor']+(1 if up else -1), ('stair failed',symbol,self.state())
        self.checkpoints.append(self.state())
    def floor(self,number):
        while self.state()['floor']!=number-1:
            self.stair(self.state()['floor']<number-1)
    def buy(self,kind):
        before=self.state();self.lib.tower_buy(self.handle,kind);self.record(['buy',kind],before)
        assert self.state()['gold']<before['gold'],'purchase failed'
    def close(self): self.lib.tower_delete(self.handle)

def route(lib,kind,early_vaults=False,secret=False,yellow_gate=False):
    name='early_vaults_recovery' if early_vaults else 'secret_detour' if secret else 'yellow_gate_alternative' if yellow_gate else kind
    r=Replay(lib,name)
    try:
        for p in [(7,9),(5,7),(5,6),(5,3),(7,4),(3,4),(8,2)]: r.go(*p)
        r.stair(True)
        for p in [(3,8),(2,2),(1,4)]:r.go(*p)
        if early_vaults:
            for p in [(6,3),(8,3),(9,3),(9,4),(6,8),(8,8),(9,8),(9,7),(9,9)]:r.go(*p)
            assert r.state()['yellow']==0
        r.go(4,1)
        r.stair(True)
        r.go(5,5) if yellow_gate else r.go(1,4)
        r.go(1,2);r.stair(True)
        for p in [(8,1),(9,1),(5,3)]:r.go(*p)
        if kind=='defence_first':
            r.floor(3)
            for p in [(6,2),(7,1),(9,1),(9,3),(8,2)]:r.go(*p)
            r.floor(4)
        for p in ([(9,5),(3,7),(9,8)] if early_vaults else [(3,5),(3,7),(9,8)]):r.go(*p)
        r.stair(True)
        if kind=='attack_first':
            for p in [(6,7),(8,7),(9,6),(9,8)]:r.go(*p)
        r.go(5,5);r.go(5,2);r.stair(True)
        r.go(3,7);r.go(7,7);r.go(5,3);r.buy(1 if kind=='defence_first' else 0);r.stair(True)
        r.go(5,5);r.go(5,6)
        if kind=='defence_first':
            r.floor(5)
            for p in [(6,7),(8,7),(9,6),(9,8)]:r.go(*p)
        else:
            r.floor(3)
            for p in [(6,2),(7,1),(9,1),(9,3),(8,2)]:r.go(*p)
        r.floor(7);r.stair(True)
        r.go(5,3);r.go(7,3)
        if secret:
            for p in [(3,6),(2,8),(2,9),(1,9)]:r.go(*p)
        r.stair(True)
        r.go(9,6);r.go(9,3);r.stair(True)
        r.go(2,8);r.go(5,3);r.go(5,2)
        before=r.state()
        assert before['chapters']&1 and not before['cleared'],('chapter prematurely ends campaign',before)
        r.stair(True)
        assert r.state()['floor']==10
        result=dict(name=name,final=r.state(),checkpoints=r.checkpoints,actions=r.moves)
        (OUT/f'{name}.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        return result
    finally:r.close()

def structural_checks():
    alphabet=set('#.ybrYBRpPadsvkXmgwMSnUDG%!')
    def reach(rows,blocked=()):
        start=next((x,y) for y,row in enumerate(rows) for x,c in enumerate(row) if c=='D')
        q=deque([start]);seen={start}
        while q:
            x,y=q.popleft()
            for p in ((x+1,y),(x-1,y),(x,y+1),(x,y-1)):
                if p in seen or p in blocked or not(0<=p[0]<11 and 0<=p[1]<11):continue
                if rows[p[1]][p[0]]=='#':continue
                seen.add(p);q.append(p)
        return seen
    for f in FLOORS:
        rows=f.rows;flat=''.join(rows)
        assert len(rows)==11 and all(len(r)==11 for r in rows)
        assert set(flat)<=alphabet and flat.count('D')==flat.count('U')==1
        assert flat.count('G')==(f.number==10)
        assert all(c=='#' for c in rows[0]+rows[-1]+''.join(r[0]+r[-1] for r in rows))
        assert len(reach(rows))==sum(c!='#' for c in flat),('disconnected',f.number)
    checks=[(3,(6,2),[(7,1),(9,1),(9,3)]),(5,(6,7),[(9,6),(9,8)]),
            (8,(2,8),[(2,9),(1,9)]),(9,(9,3),[(9,1)]),(10,(5,3),[(5,2),(5,1)])]
    for floor,barrier,rewards in checks:
        accessible=reach(FLOORS[floor-1].rows,[barrier])
        assert all(p not in accessible for p in rewards),('bypass',floor,barrier,rewards)
    all_rows=''.join(''.join(f.rows) for f in FLOORS)
    assert all_rows.count('b')==2 and all_rows.count('B')==2
    assert all_rows.count('r')==all_rows.count('R')==1
    content=json.loads((ROOT/'assets_src/level_design/region1_content.json').read_text(encoding='utf-8'))
    for npc in content['npcs']:
        x,y=npc['position']
        assert FLOORS[npc['floor']-1].rows[y][x]=='n',('NPC coordinate mismatch',npc)
        assert all(len(line)<=17 for line in npc['zh']) and all(len(line)<=38 for line in npc['en'])
    for language,limit in [('zh',17),('en',38)]:
        assert all(len(line)<=limit for line in content['chapter_gate_text'][language])
    assert FLOORS[9].rows[3][5]=='k' and FLOORS[9].rows[2][5]=='G' and FLOORS[9].rows[1][5]=='U'
    print('PASS: geometry, two blue vaults, secret, red seal and compulsory boss')

def greedy_failure(lib):
    r=Replay(lib,'greedy_lethal_confirm')
    try:
        for p in [(7,9),(5,7),(5,6),(3,3)]:r.go(*p)
        before=r.state()
        # Knight is visibly lethal; accepting it is the intended losing choice.
        r.lib.tower_move(r.handle,-1,0)
        assert r.state()['phase']==1 and not r.state()['winnable']
        preview=r.state()
        r.record(['preview_lethal_knight'],before)
        r.lib.tower_confirm(r.handle)
        r.record(['confirm_lethal_knight'],preview)
        assert r.state()['hp']==0 and r.lib.lab_is_knight(r.handle,2,3)
        (OUT/'greedy_lethal_confirm.json').write_text(json.dumps(dict(preview=preview,final=r.state()),indent=2))
        print('PASS: greedy lethal fight warns, then kills player and retains knight')
    finally:r.close()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--export',action='store_true',help='export designer-owned JSON and C include; never touch production')
    args=parser.parse_args()
    data=[f.data() for f in FLOORS]
    (OUT/'maps.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    structural_checks()
    lib=build()
    results=[route(lib,name) for name in ('defence_first','attack_first')]
    results += [route(lib,'defence_first',early_vaults=True),route(lib,'defence_first',secret=True)]
    results += [route(lib,'defence_first',yellow_gate=True)]
    assert results[0]['final']['hp']==130 and results[1]['final']['hp']==135
    assert results[3]['final']['hp']==results[0]['final']['hp']+80
    assert results[4]['final']['hp']==results[0]['final']['hp']+18
    assert results[4]['final']['yellow']==results[0]['final']['yellow']-1
    assert results[4]['final']['gold']==results[0]['final']['gold']-6
    greedy_failure(lib)
    (OUT/'summary.json').write_text(json.dumps([dict(name=r['name'],final=r['final'],steps=len(r['actions'])) for r in results],indent=2))
    for r in results:
        s=r['final']
        print(f"PASS: {r['name']}: HP{s['hp']} ATK{s['attack']} DEF{s['defence']} keys={s['yellow']}/{s['blue']}/{s['red']} gold={s['gold']} actions={len(r['actions'])}")
    if args.export:
        target=ROOT/'assets_src/level_design'
        (target/'region1_maps.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        lines=['/* Designer-owned Region I. Generated by tools/verify_region1_design.py --export.',
               ' * Replace only the first ten row definitions after integration review.',
               ' * Uses existing tile alphabet; not a standalone 50-floor campaign. */']
        for f in FLOORS:
            lines += [f'/* {f.number}F */',f'static const char *const tower_floor_{f.number:02d}_rows[] = {{']
            lines += ['    '+json.dumps(row)+',' for row in f.rows]+['};','']
        (target/'region1_floors.inc').write_text('\n'.join(lines),encoding='utf-8')
        print('EXPORTED: region1_maps.json + region1_floors.inc; production files untouched')

if __name__=='__main__':main()
