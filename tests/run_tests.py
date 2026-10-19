"""Portable fifty-floor test entry point; legacy ten-floor harness is archived."""
import argparse
from pathlib import Path
import subprocess
import sys
p=argparse.ArgumentParser();p.add_argument('--label',default='local');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
commands=[['tests/run_tower50.py','--label',a.label],['tests/test_campaign_production.py'],
          ['tests/test_campaign_game.py'],['tools/test_campaign_contract.py']]
for command in commands:
    subprocess.run([sys.executable,*command],cwd=root,check=True)
