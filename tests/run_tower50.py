"""Build and run the 50-floor portable tower model tests without ESP-IDF."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--label", default="tower50")
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
artifacts = project / "artifacts"
artifacts.mkdir(exist_ok=True)
gcc = Path(os.environ.get("CC") or shutil.which("gcc") or "gcc")
output = artifacts / "test_tower50.exe"
command = [str(gcc), "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
           "-I", str(project / "main"), "-I", str(project / "tests"),
           str(project / "tests/test_tower50.c"),
           str(project / "main/tower.c"),
           str(project / "tests/tower_floors_stub.c"),
           "-o", str(output)]
if not gcc.exists():
    raise SystemExit(f"gcc not found: {gcc}. Set CC or add gcc to PATH.")
build = subprocess.run(command, capture_output=True, text=True)
log = "COMPILE: " + " ".join(command) + "\n" + build.stdout + build.stderr
result = build.returncode
if not result:
    run = subprocess.run([str(output)], capture_output=True, text=True)
    log += "RUN:\n" + run.stdout + run.stderr
    result = run.returncode
log += f"EXIT: {result}\n"
(artifacts / f"tower50-tests-{args.label}.log").write_text(log, encoding="utf-8")
print(log)
raise SystemExit(result != 0)
