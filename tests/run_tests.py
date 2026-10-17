"""Build and run the portable C model without ESP-IDF or external packages."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--label", default="green")
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
artifacts = project / "artifacts"
artifacts.mkdir(exist_ok=True)
gcc = Path(os.environ.get("CC") or shutil.which("gcc") or "gcc")
output = artifacts / "test_tower.exe"
command = [str(gcc), "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-g",
           "-I", str(project / "main"), "-I", str(project / "tests"),
           str(project / "tests/test_tower.c"),
           str(project / "main/tower.c"), "-o", str(output)]
if not gcc.exists():
    raise SystemExit(f"gcc not found: {gcc}. Set CC or add gcc to PATH.")
env = os.environ.copy()
env["PATH"] = str(gcc.parent) + os.pathsep + env.get("PATH", "") if gcc.parent != Path(".") else env.get("PATH", "")
build = subprocess.run(command, capture_output=True, text=True, env=env)
log = "COMPILE: " + " ".join(command) + "\n" + build.stdout + build.stderr
result = build.returncode
if not result:
    run = subprocess.run([str(output)], capture_output=True, text=True, env=env)
    log += "RUN:\n" + run.stdout + run.stderr
    result = run.returncode
log += f"EXIT: {result}\n"
(artifacts / f"model-tests-{args.label}.log").write_text(log, encoding="utf-8")
print(log)
raise SystemExit(result != 0)
