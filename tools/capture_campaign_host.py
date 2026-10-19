"""Replay production campaign inputs through the official Host ABI and renderer.

The sources are copied verbatim into an isolated project so its real storage
adapter cannot touch a user's simulator checkpoint. Python never renders tiles.
Run tests/test_campaign_production.py first to generate the approved input trace.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/campaign-host'
sys.path.insert(0,str(ROOT/'assets_src/level_design'))
from campaign_rules import HIDDEN
from capture_campaign_design import engine_root

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    engine=engine_root(os.environ.get('MOSAICO_ENGINE_ROOT'))
    spec=importlib.util.spec_from_file_location('campaign_official_host',engine/'host/run_game.py')
    host=importlib.util.module_from_spec(spec);sys.modules[spec.name]=host;spec.loader.exec_module(host)
    cc=os.environ.get('CC') or shutil.which('gcc') or r'D:\espbulid\mingw64\bin\gcc.exe'
    os.environ['PATH']=str(Path(cc).parent)+os.pathsep+os.environ.get('PATH','')
    trace=json.loads((ROOT/'artifacts/campaign-production/secrets_and_errands.json').read_text(encoding='utf-8'))
    snapshots=[];floors=set();dialogs=set();hidden=set()
    mouths={(f,*entry) for f,entry,_ in HIDDEN}
    with tempfile.TemporaryDirectory(prefix='production-',dir=OUT) as temporary:
        project=Path(temporary);(project/'main').mkdir()
        sources={}
        for path in (ROOT/'main').iterdir():
            if path.suffix in ('.c','.h'):
                shutil.copy2(path,project/'main'/path.name)
                sources[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        shutil.copy2(ROOT/'game.sim.json',project/'game.sim.json')
        runtime=host.GenericHostRuntime(project,project)
        def state():return runtime.metadata()
        def key(code):
            runtime.action(code,True);runtime.action(code,False);runtime.step(False,False,False)
        def tap(x,y):
            runtime.pointer(0,x,y,True);runtime.pointer(0,x,y,False);runtime.step(False,False,False)
        def shot(name):
            (OUT/(name+'.png')).write_bytes(runtime.frame())
            snapshots.append(dict(file=name+'.png',state=state()))
        try:
            assert state()['screen']=='title';shot('title')
            tap(437,30);shot('settings-zh');tap(350,158);shot('settings-en')
            tap(250,158);tap(390,102);key(6)
            tap(292,462);assert state()['screen']=='restart_menu';shot('restart-zh');key(0)
            for a in trace['actions']:
                while state()['screen']=='dialog':
                    s=state();identity=s['dialogue_id']
                    if identity not in dialogs:
                        shot(f'dialog-{identity:02d}-zh');dialogs.add(identity)
                    if (s['floor'],s['x'],s['y'])==(46,12,14):
                        shot(f'gpt-page-{s["dialogue_page"]}-zh')
                    key(6)
                s=state()
                if s['screen']=='victory':shot('victory');key(6)
                action=a['action'];kind=action[0]
                if kind in ('move','warp') and s['screen']=='shop':key(0)
                target=(s['floor'],s['x']+action[1],s['y']+action[2]) if kind=='move' else None
                discover=target in mouths and target not in hidden
                if discover:shot(f'hidden-{target[0]+1:02d}-before')
                if kind=='move':key({(-1,0):0,(1,0):1,(0,-1):2,(0,1):5}[tuple(action[1:])])
                elif kind=='confirm':key(6)
                elif kind=='buy':tap(230,240 if action[1]==0 else 282)
                elif kind=='warp':
                    tap(185,460);index=action[1]
                    tap(78+(index%2)*174+75,152+(index//2)*70+28)
                else:raise AssertionError(action)
                s=state();expected=a['after']
                for name in ('floor','x','y','hp','attack','defence','gold','turns','cleared','chapters'):
                    assert s[name]==expected[name],(action,name,s[name],expected[name])
                if discover and (s['floor'],s['x'],s['y'])==target:
                    shot(f'hidden-{target[0]+1:02d}-after');hidden.add(target)
                if s['floor'] not in floors:
                    shot(f'floor-{s["floor"]+1:02d}');floors.add(s['floor'])
                if s['floor']==49 and s['damage'] and not s['cleared']:shot('throne-combat')
            assert floors==set(range(50)) and len(hidden)==8 and state()['cleared']
            shot('ending-explore')
            tap(185,460);shot('all-anchors')
        finally:runtime.close()
    report=dict(kind='Production sources verbatim; official Host ABI; real input replay and RGB565 renderer',
                engine=str(engine),source_sha256=sources,actions=len(trace['actions']),
                floors=len(floors),hidden_rooms=len(hidden),dialogs=len(dialogs),screenshots=snapshots)
    (OUT/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'PASS: official Host ABI full route; {len(floors)} floors, {len(hidden)} hidden reveals, {len(dialogs)} dialogs, {len(snapshots)} screenshots')

if __name__=='__main__':main()
