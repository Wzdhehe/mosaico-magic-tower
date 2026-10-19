"""Production tables and model must match the approved 50-floor campaign."""
from pathlib import Path
import ctypes as C
import json
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'assets_src/level_design'))
from tower_design_lab import build
from verify_campaign_design import FLOORS, run_route

def main():
    approved=(ROOT/'assets_src/level_design/campaign_floors.c').read_text(encoding='utf-8')
    current=(ROOT/'main/tower_floors.c').read_text(encoding='utf-8')
    assert current==approved,'Production floor table has not integrated the approved maps'
    out=ROOT/'artifacts/campaign-production'
    lib=build(FLOORS,out,production_table=True)
    lib.tower_sluice_south.argtypes=[C.c_void_p]
    lib.tower_sigils.argtypes=[C.c_void_p]
    lib.tower_quest_state.argtypes=[C.c_void_p,C.c_uint]
    import verify_campaign_design as witness
    witness.OUT=out
    expected={'defence':(165,177,124,3,1,0,472),'attack':(10,198,109,2,1,0,346),
              'early_vault_recovery':(943,182,127,2,1,0,493),
              'secrets_and_errands':(1399,223,118,5,1,0,372),
              'yellow_detour':(183,177,124,2,1,0,466)}
    options=[('defence','defence',{}),('attack','attack',{}),
             ('early_vault_recovery','defence',dict(early=True)),
             ('secrets_and_errands','attack',dict(secrets=True,sidequests=True)),
             ('yellow_detour','defence',dict(yellow=True))]
    results=[]
    for name,kind,flags in options:
        r=run_route(lib,name,kind,**flags);s=r['final']
        actual=tuple(s[k] for k in ('hp','attack','defence','yellow','blue','red','gold'))
        assert actual==expected[name],(name,actual,expected[name])
        assert s['cleared'] and s['chapters']==31
        results.append(r);print('PASS: production',name,actual)
    (out/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print('PASS: real production model/table, five complete routes, each action save roundtrip')
if __name__=='__main__':main()
