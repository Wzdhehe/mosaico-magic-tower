"""49F removed door stays open, including v8 saves that spent its yellow key."""
import ctypes as C
from copy import deepcopy
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from tower_design_lab import build,Replay
from verify_campaign_design import FLOORS

def main():
    out=ROOT/'artifacts/rc4-door-save'
    current=build(FLOORS,out/'current',production_table=True)
    assert current.lab_width(48)==19
    h=C.c_void_p();assert current.tower_create(None,C.byref(h))==0
    current.tower_delete(h)
    current.tower_tile.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_int]
    assert current.tower_create(None,C.byref(h))==0
    try:assert current.tower_tile(h,48,15,3)==0,'49F redundant yellow door still exists'
    finally:current.tower_delete(h)
    old=deepcopy(FLOORS);old[48].grid[3][15]='Y'
    legacy=build(old,out/'rc3-map')
    # All other model/save rules are unchanged. The original row character
    # produces a real opened-door flag and real key spending, not a forged blob.
    for lib in (legacy,current):
        lib.tower_export.argtypes=[C.c_void_p,C.POINTER(C.c_uint8),C.c_size_t]
        lib.tower_import.argtypes=[C.c_void_p,C.POINTER(C.c_uint8),C.c_size_t]
        lib.tower_sluice_south.argtypes=[C.c_void_p]
        lib.tower_sigils.argtypes=[C.c_void_p]
        lib.tower_quest_state.argtypes=[C.c_void_p,C.c_uint]
    import verify_campaign_design as witness
    witness.OUT=out/'trace'
    # Keep the actual legacy handle for the unopened/opened versions.
    from verify_campaign_design import chapter1,chapter2,chapter3,chapter4,chapter5
    with Replay(legacy,old,out/'trace','legacy') as r:
        for chapter in (chapter1,chapter2,chapter3,chapter4,chapter5):chapter(r,'defence')
        r.floor(49)
        def export(lib,handle):
            b=(C.c_uint8*2632)();assert lib.tower_export(handle,b,len(b))==0;return b
        unopened=export(legacy,r.handle);keys=r.state()['yellow']
        r.go(15,3);assert r.state()['yellow']==keys-1
        opened=export(legacy,r.handle)
        for blob in (unopened,opened):
            with Replay(current,FLOORS,out/'trace','restored') as restored:
                assert current.tower_import(restored.handle,blob,len(blob))==0,'Valid RC3 v8 save rejected'
                assert bytes(export(current,restored.handle))==bytes(blob),'Migration altered resources/checkpoint'
                before=restored.state()['yellow'];restored.go(15,3)
                assert restored.state()['yellow']==before,'Removed door still consumes a yellow key'
                assert current.lab_roundtrip(restored.handle)==0
    print('PASS: 49F door removed; genuine unopened/opened RC3 v8 saves import and roundtrip without extra key spending')
if __name__=='__main__':main()
