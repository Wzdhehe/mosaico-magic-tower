"""Run the current campaign validation suite and retain bounded per-check logs."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/campaign-validation'

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy()
    cc=env.get('CC') or shutil.which('gcc')
    fallback=Path('D:/espbulid/mingw64/bin/gcc.exe')
    if not cc and fallback.is_file():cc=str(fallback)
    if cc:
        env['CC']=cc;env['PATH']=str(Path(cc).parent)+os.pathsep+env.get('PATH','')
    temporary=OUT/'tmp';temporary.mkdir(exist_ok=True)
    env.update(TEMP=str(temporary),TMP=str(temporary),PYTHONIOENCODING='utf-8')
    checks=[('generated-dialogue',['tools/gen_campaign_dialogue.py','--check']),
            ('generated-music',['tools/gen_campaign_music.py','--check']),
            ('glyphs',['tests/test_ui_glyphs.py']),
            ('model-save',['tests/run_tower50.py','--label','campaign-release']),
            ('production-routes',['tests/test_campaign_production.py']),
            ('campaign-contracts',['tools/test_campaign_contract.py']),
            ('actual-game-input',['tests/test_campaign_game.py']),
            ('retired-door-v8',['tests/test_rc4_door_save.py']),
            ('combat-stat-ink',['tests/test_rc4_combat.py']),
            ('restart',['tests/run_restart_menu.py']),
            ('battery',['tests/run_battery.py']),
            ('package-schema',['tests/test_package_firmware.py']),
            ('camera',['tests/run_camera_controls.py']),
            ('timing',['tests/run_game_timing.py']),
            ('preferences',['tests/run_preferences_storage.py']),
            ('music',['tests/run_game_music.py']),
            ('audio-power',['tests/run_audio_power.py']),
            ('text-layout',['tests/test_ui_layout.py']),
            ('display-control',['tests/run_display_control.py']),
            ('render-schedule',['tests/run_render_schedule.py']),
            ('official-host-route',['tools/capture_campaign_host.py'])]
    results=[]
    for name,args in checks:
        start=time.monotonic()
        r=subprocess.run([sys.executable,*args],cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
        (OUT/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
        results.append(dict(name=name,args=args,exit=r.returncode,seconds=round(time.monotonic()-start,3)))
        print(('PASS' if r.returncode==0 else 'FAIL')+': '+name,flush=True)
        report=dict(checks=results,production_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted((ROOT/'main').iterdir()) if p.suffix in ('.c','.h')})
        (OUT/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        if r.returncode:
            print((r.stdout+r.stderr)[-3500:]);return 1
    print(f'PASS: {len(results)} campaign validation commands; logs: {OUT}')
    return 0

if __name__=='__main__':raise SystemExit(main())
