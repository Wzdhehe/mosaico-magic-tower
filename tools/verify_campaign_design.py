"""Fifty authored floors, real C replay, and exportable exact integration data.

No solver silently collects everything. All routes below are designer-owned
waypoints; C alone resolves combat, inventory, events, and every save roundtrip.
"""
from collections import deque, Counter
import argparse
import ctypes as C
import difflib
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets_src/level_design'))
from region1_blueprint import FLOORS as R1
from region2_blueprint import FLOORS as R2
from region3_blueprint import FLOORS as R3
from region4_blueprint import FLOORS as R4
from region5_blueprint import FLOORS as R5
from campaign_rules import transform, BOSSES, REGIONAL_ENEMIES, QUESTS, HIDDEN
from campaign_content import data as content_data
from campaign_presentation import transform_view
from tower_design_lab import build, Replay
FLOORS=R1+R2+R3+R4+R5
OUT=ROOT/'artifacts/campaign-design'

def points(r,*coords):
    for p in coords:r.go(*p)

def chapter1(r,kind,early=False,secret=False,yellow=False):
    points(r,(7,9),(5,7),(5,6),(5,3),(7,4),(3,4),(8,2));r.stair(True)
    points(r,(3,8),(2,2),(1,4))
    if early:points(r,(6,3),(8,3),(9,3),(9,4),(6,8),(8,8),(9,8),(9,7),(9,9))
    r.go(4,1);r.stair(True)
    r.go(5,5) if yellow else r.go(1,4)
    r.go(1,2);r.stair(True)
    points(r,(8,1),(9,1),(5,3))
    if kind=='defence':
        r.floor(3);points(r,(6,2),(7,1),(9,1),(9,3),(8,2));r.floor(4)
    points(r,*([(9,5),(3,7),(9,8)] if early else [(3,5),(3,7),(9,8)]));r.stair(True)
    if kind=='attack':points(r,(6,7),(8,7),(9,6),(9,8))
    points(r,(5,5),(5,2));r.stair(True)
    points(r,(3,7),(7,7),(5,3));r.buy(kind=='defence');r.stair(True)
    points(r,(5,5),(5,6))
    r.floor(5 if kind=='defence' else 3)
    points(r,*([(6,7),(8,7),(9,6),(9,8)] if kind=='defence' else [(6,2),(7,1),(9,1),(9,3),(8,2)]))
    r.floor(7);r.stair(True);points(r,(5,3),(7,3))
    if secret:points(r,(3,6),(2,7),(2,9),(1,9))
    r.stair(True);points(r,(9,6),(9,3));r.stair(True)
    points(r,(2,8),(5,3),(5,2));r.checkpoint('chapter1');r.stair(True)

def chapter2(r,kind,sidequests=False):
    points(r,(1,10),(4,11),(3,9),(5,8),(5,6));r.stair(True)
    points(r,(7,9),(3,6),(3,5),(5,1),(4,3));r.stair(True)
    points(r,(2,9),(3,11),(7,11))
    if kind=='defence':r.go(8,9)
    else:points(r,(6,1),(11,5))
    r.go(11,10);r.stair(True)
    points(r,(2,5),(4,5));r.stair(True)
    # Take north vault, return to the visible valve, open south vault.
    points(r,(5,2),(7,3),(9,5),(9,3));r.floor(14);r.go(3,4)
    assert r.lib.tower_sluice_south(r.handle)==1
    r.floor(15);points(r,(1,6),(1,8),(5,10),(7,7),(9,9),(9,7));r.stair(True)
    points(r,(3,6),(5,4));r.go(6,10) if kind=='defence' else r.go(6,2)
    points(r,(7,8),(10,10) if kind=='defence' else (10,2));r.stair(True)
    points(r,(6,5),(9,3),(9,5),(8,7),(5,9),(9,11),(9,9));r.buy(kind=='defence');r.stair(True)
    points(r,(5,1),(9,1),(11,4),(11,8))
    if sidequests:
        points(r,(3,5),(4,5),(7,7),(5,7))
        assert r.lib.tower_quest_state(r.handle,0)==1
        r.warp(1);r.floor(11);points(r,(1,9),(6,9),(9,7),(11,9));r.floor(18)
        assert r.lib.tower_quest_state(r.handle,0)==2
    r.stair(True)
    points(r,(5,7),(9,5),(11,7),(11,3),(5,3));r.stair(True)
    points(r,(3,7),(7,7),(11,3),(11,2));r.checkpoint('chapter2');r.stair(True)

def chapter3(r,kind,secrets=False):
    points(r,(3,12),(13,11),(11,9),(9,7),(11,4));r.stair(True)
    points(r,(11,3),(9,5),(7,5),(5,7),(5,11));r.stair(True)
    points(r,(1,10),(1,5),(3,7),(5,5),(7,9),(7,7),(5,1),(13,5),(13,8))
    # Spend the first blue key on the 21F permanent-growth stock.
    r.floor(21);points(r,(5,11),(7,8),(5,5),(7,5),(5,7));r.floor(23);r.stair(True)
    points(r,(3,3),(7,3))
    if kind=='defence':points(r,(7,7),(11,7))
    else:r.go(8,1)
    points(r,(9,11),(13,11));r.stair(True)
    points(r,(3,11),(7,11),(1,5),(5,9));r.buy(kind=='defence');r.stair(True)
    points(r,(5,7),(7,5));r.go(1,7) if kind=='defence' else r.go(13,7)
    points(r,(9,7),(2,13) if kind=='defence' else (12,13));r.stair(True)
    points(r,(1,7),(3,5),(7,7),(5,6),(7,9),(11,11))
    # The second blue key buys 28F reserve; optional gold room on 23F remains.
    r.stair(True);points(r,(5,9),(3,5),(5,5),(3,7),(1,5))
    if secrets:points(r,(11,9),(11,8),(11,5),(13,5),(9,5))
    r.stair(True);points(r,(3,11),(7,11),(9,7),(9,11),(11,9),(1,7));r.stair(True)
    points(r,(3,7),(5,11),(13,5),(13,6));r.checkpoint('chapter3');r.stair(True)

def chapter4(r,kind,secrets=False,sidequests=False):
    points(r,(8,12),(4,11),(12,11),(3,9),(2,7),(13,9),(14,11))
    r.go(4,6) if kind=='defence' else r.go(12,6)
    r.stair(True)
    # Tax vault: one yellow key buys the chance to earn 18 gold from bats.
    points(r,(5,3),(3,6),(5,6),(7,6),(3,7),(7,7),(15,5),(15,6),(15,10));r.stair(True)
    points(r,(1,14),(5,3),(8,7),(5,5),(7,5));r.stair(True)
    points(r,(3,5),(3,7))
    r.go(1,6);r.go(1,10) if kind=='defence' else None
    if kind=='attack':r.go(15,8)
    points(r,(13,9),(13,11),(2,15) if kind=='defence' else (14,15));r.stair(True)
    points(r,(3,13),(1,11),(5,11),(15,7),(15,3),(11,9),(13,11),(9,11));r.stair(True)
    points(r,(9,9))
    if kind=='defence':points(r,(1,8))
    else:points(r,(15,8),(9,11))
    points(r,(3,11));r.stair(True)
    points(r,(1,10),(3,5))
    if secrets:points(r,(5,6),(3,9),(7,9))
    points(r,(9,5),(13,7));r.stair(True)
    points(r,(1,10),(1,6),(5,11),(3,7),(7,7),(5,7));r.stair(True)
    points(r,(7,7),(7,11),(9,15),(11,7),(13,11),(13,9));r.buy(kind=='defence')
    if sidequests:
        # Return the found shadow to the first workshop and use its disclosed cache.
        r.warp(2);r.floor(21);points(r,(3,10),(3,8),(1,5),(1,7),(3,5));r.floor(39)
        assert r.lib.tower_quest_state(r.handle,1)==2
    r.stair(True);points(r,(7,7),(9,11),(1,3),(1,4));r.checkpoint('chapter4');r.stair(True)

def chapter5(r,kind,secrets=False,sidequests=False):
    points(r,(7,16),(3,11),(3,13),(7,11),(10,9),(13,3));r.stair(True)
    points(r,(1,9),(1,10),(1,12));r.go(7,3) if kind=='defence' else r.go(11,7)
    points(r,(5,5),(9,9));r.stair(True)
    points(r,(7,17))
    r.go(1,9) if kind=='defence' else r.go(17,9)
    points(r,(9,7),(13,11),(13,7),(1,5) if kind=='defence' else (17,5));r.stair(True)
    points(r,(13,3),(11,5),(15,7),(13,7),(5,3),(3,5),(7,7),(5,7),(5,11),(7,9),(7,13),(1,14));r.stair(True)
    points(r,(1,5),(3,3),(3,13),(9,9),(9,13),(7,11))
    for _ in range(1 if kind=='defence' else 3):r.buy(kind=='defence')
    r.stair(True)
    points(r,(3,3),(11,9),(17,8),(17,13),(7,17),(5,15))
    if sidequests:
        r.warp(3);r.floor(33);points(r,(9,5),(10,11),(11,15),(13,13),(13,15));r.floor(46)
        assert r.lib.tower_quest_state(r.handle,2)==2
    r.stair(True);points(r,(5,15))
    if secrets:
        assert r.lib.tower_sigils(r.handle)==7
        points(r,(9,11),(11,13),(13,15),(11,15),(13,13))
    r.go(11,1);r.stair(True)
    points(r,(3,7),(7,7))
    if kind=='defence':r.go(1,9)
    else:points(r,(17,7),(17,11))
    points(r,(11,11),(15,11),(1,15) if kind=='defence' else (17,15));r.stair(True)
    points(r,(1,5),(7,7),(9,11),(9,7),(7,11));r.stair(True)
    points(r,(2,4),(8,4),(2,7),(8,7));r.checkpoint('before_final')
    r.go(5,3);assert r.state()['cleared'] and r.state()['chapters']==31
    r.checkpoint('chapter5');r.go(5,2)
    # Every anchor remains usable after victory, including the real final floor.
    for anchor in range(6):
        r.warp(anchor);assert r.state()['floor']==[0,9,19,29,39,49][anchor]
    r.checkpoint('victory_warps')

def reach(f,blocked=()):
    rows=f.rows;size=len(rows);start=next((x,y) for y,row in enumerate(rows) for x,c in enumerate(row) if c=='D')
    q=deque([start]);seen={start}
    while q:
        x,y=q.popleft()
        for v in ((x+1,y),(x-1,y),(x,y+1),(x,y-1)):
            if v in seen or v in blocked or not(0<=v[0]<size and 0<=v[1]<size) or rows[v[1]][v[0]]=='#':continue
            seen.add(v);q.append(v)
    return seen

def structural():
    assert len(FLOORS)==50 and len(set(tuple(f.rows) for f in FLOORS))==50
    for f in FLOORS:
        rows=f.rows;flat=''.join(rows);size=11 if f.number==50 else 11+2*((f.number-1)//10)
        assert len(rows)==size and all(len(row)==size for row in rows)
        assert set(flat)<=set('#.ybrYBRpPadsvkXmgwMSnUDG%!')
        assert flat.count('D')==1 and flat.count('U')==(f.number<50),(f.number,'stairs')
        assert flat.count('G')==(f.number%10==0),(f.number,'goal')
        assert all(c=='#' for c in rows[0]+rows[-1]+''.join(row[0]+row[-1] for row in rows))
        seen=reach(f);missing=[(x,y) for y,row in enumerate(rows) for x,c in enumerate(row) if c!='#' and (x,y) not in seen]
        assert not missing,(f.number,'disconnected',missing)
    guards=[(10,(5,3)),(20,(11,3)),(30,(13,5)),(40,(1,3)),(50,(5,3))]
    gates=[(8,(2,7),[(2,9),(1,9)]),(18,(4,5),[(7,7)]),(28,(11,8),[(11,5)]),
           (37,(5,6),[(3,9),(7,9)]),(47,(9,11),[(11,15),(13,13)]),
           (11,(6,9),[(9,7),(11,9)]),(21,(3,8),[(1,5),(1,7)]),(33,(10,11),[(11,15),(13,15)]),
           (15,(5,2),[(7,3),(9,5)]),(15,(5,10),[(7,7),(9,9)])]
    for floor,guard in guards:
        seen=reach(FLOORS[floor-1],[guard]);rows=FLOORS[floor-1].rows
        assert all((x,y) not in seen for y,row in enumerate(rows) for x,c in enumerate(row) if c in 'UG'),('boss bypass',floor)
    for floor,gate,loot in gates:
        assert not any(v in reach(FLOORS[floor-1],[gate]) for v in loot),('secret/gate bypass',floor,gate,loot)
    for floor,gate,loot in [(3,(6,2),[(7,1),(9,3)]),(5,(6,7),[(9,6),(9,8)]),
                            (9,(9,3),[(9,1)]),(19,(5,3),[(1,1)]),(29,(1,7),[(1,13)]),
                            (39,(7,7),[(15,15)]),(49,(1,5),[(11,5)])]:
        assert not any(v in reach(FLOORS[floor-1],[gate]) for v in loot),('seal/vault bypass',floor)
    content=content_data()
    positions={(n['floor']-1,*n['position']) for n in content['npcs']}
    actual={(f.number-1,x,y) for f in FLOORS for y,row in enumerate(f.rows) for x,c in enumerate(row) if c=='n'}
    assert actual==positions,('NPC coverage',actual-positions,positions-actual)
    for n in content['npcs']:
        pages=n['pages']+n.get('extra_pages',[])+[n[k] for k in ('carrying','done') if k in n]
        for p in pages:
            assert len(p['zh'])==len(p['en'])==2
            assert all(len(s)<=17 for s in p['zh']),('Chinese page too long',n,p)
            assert all(len(s)<=38 for s in p['en']),('English page too long',n,p)
    for floor,entry,rects in HIDDEN:
        assert sum((x2-x1+1)*(y2-y1+1) for x1,y1,x2,y2 in rects)>=6
        assert FLOORS[floor].rows[entry[1]][entry[0]]=='%'
    for score in content['scores'].values():
        assert sum(int(s.split(':')[1]) for s in score['lead'].split())==32
        assert sum(int(s.split(':')[1]) for s in score['bass'].split())==32
    assert len(FLOORS[49].rows)==11
    assert all((FLOORS[49].rows[y][x]=='#')==(FLOORS[49].rows[y][10-x]=='#') for y in range(11) for x in range(11)), 'throne architecture lost symmetry'
    print('PASS: 50 distinct bounded maps; stairs, goals, connectivity, five mandatory bosses, eight hidden rooms, water gates')

def export():
    target=ROOT/'assets_src/level_design';target.mkdir(exist_ok=True)
    (target/'campaign_maps.json').write_text(json.dumps([f.data() for f in FLOORS],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    lines=['/* Authored fifty-floor campaign. Generated by verify_campaign_design.py. */','#include "tower_floors.h"','']
    for f in FLOORS:
        lines += [f'/* {f.number}F */',f'static const char *const tower_floor_{f.number:02d}_rows[] = {{']
        lines += ['    '+json.dumps(row)+',' for row in f.rows]+['};','']
    lines+=['const tower_floor_desc_t tower_floor_table[TOWER_FLOORS] = {']
    lines += [f'    {{{len(f.rows)}, {len(f.rows)}, tower_floor_{f.number:02d}_rows}},' for f in FLOORS]+['};','']
    (target/'campaign_floors.c').write_text('\n'.join(lines),encoding='utf-8')
    original=(ROOT/'main/tower.c').read_text(encoding='utf-8');changed=transform(original)
    patch=''.join(difflib.unified_diff(original.splitlines(True),changed.splitlines(True),fromfile='a/main/tower.c',tofile='b/main/tower.c'))
    (target/'campaign_model.patch').write_text(patch,encoding='utf-8')
    content=content_data()
    content['hidden_rooms']=[dict(floor=f+1,entry=list(g),rectangles=[list(r) for r in rects]) for f,g,rects in HIDDEN]
    content['quests']=[dict(id=i,npc=[n[0]+1,*n[1:]],item=[item[0]+1,*item[1:]],reward_entry=[gate[0]+1,*gate[1:]]) for i,(n,item,gate) in enumerate(QUESTS)]
    content['enemy_profiles']={'region2_to_5':REGIONAL_ENEMIES,'bosses':BOSSES}
    (target/'campaign_content.json').write_text(json.dumps(content,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    original=(ROOT/'main/game_view.c').read_text(encoding='utf-8');changed=transform_view(original)
    patch=''.join(difflib.unified_diff(original.splitlines(True),changed.splitlines(True),fromfile='a/main/game_view.c',tofile='b/main/game_view.c'))
    (target/'campaign_view.patch').write_text(patch,encoding='utf-8')

def run_route(lib,name,kind,early=False,secrets=False,sidequests=False,yellow=False):
    with Replay(lib,FLOORS,OUT,name) as r:
        chapter1(r,kind,early,secrets,yellow)
        chapter2(r,kind,sidequests)
        chapter3(r,kind,secrets)
        chapter4(r,kind,secrets,sidequests)
        chapter5(r,kind,secrets,sidequests)
        return dict(name=name,final=r.state(),checkpoints=r.checkpoints,actions=len(r.moves))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--export',action='store_true');parser.add_argument('--route',default='all');args=parser.parse_args()
    structural()
    if args.export:export()
    lib=build(FLOORS,OUT,transform)
    lib.tower_sluice_south.argtypes=[C.c_void_p];lib.tower_sigils.argtypes=[C.c_void_p];lib.tower_quest_state.argtypes=[C.c_void_p,C.c_uint]
    options=[('defence','defence',{}),('attack','attack',{}),('early_vault_recovery','defence',dict(early=True)),
             ('secrets_and_errands','attack',dict(secrets=True,sidequests=True)),('yellow_detour','defence',dict(yellow=True))]
    results=[]
    for name,kind,kw in options:
        if args.route not in ('all',name):continue
        result=run_route(lib,name,kind,**kw);results.append(result)
        print('PASS:',name,{k:result['final'][k] for k in ('hp','attack','defence','yellow','blue','red','gold','cleared','chapters')},'actions',result['actions'])
    assert results,('unknown route',args.route)
    (OUT/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print('All selected routes reached the real 50F ending; every action roundtripped through the real C save validator.')
if __name__=='__main__':main()
