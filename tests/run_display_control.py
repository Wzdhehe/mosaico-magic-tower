"""Exercise the actual workspace engine brightness transaction with fake hardware."""
import os
from pathlib import Path
import re
import shutil
import subprocess

project = Path(__file__).resolve().parents[1]
engine = Path(os.environ.get("MOSAICO_ENGINE_ROOT", project.parents[1] / "submodule/raylib-lite-engine"))
source = Path(os.environ.get("MOSAICO_DISPLAY_CONTROL_SOURCE", str(engine / "components/mosaico_game_app/mosaico_game_app.c")))
artifacts = project / "artifacts/display-control"
artifacts.mkdir(parents=True, exist_ok=True)
code = source.read_text(encoding="utf-8")

def function(name):
    match = re.search(r"(?:static )?esp_err_t\s+" + name + r"\([^;]*?\)\s*\{", code)
    if not match:
        raise SystemExit(f"FAIL: engine lacks serialized {name}")
    depth = 1
    pos = match.end()
    while depth:
        depth += (code[pos] == "{") - (code[pos] == "}")
        pos += 1
    return code[match.start():pos]

functions = function("resume_brightness_display") + "\n" + function("mosaico_game_app_set_brightness")
timeout = re.search(r"enum\s*\{\s*MOSAICO_BRIGHTNESS_PAUSE_TIMEOUT_MS\s*=\s*\d+\s*\};", code)
if not timeout:
    raise SystemExit("FAIL: engine lacks named bounded brightness timeout")
(artifacts / "display_control_under_test.inc").write_text(timeout.group() + "\n" + functions, encoding="utf-8")
gcc = os.environ.get("CC") or shutil.which("gcc") or "gcc"
env = os.environ.copy()
env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")
env["TEMP"] = env["TMP"] = str(artifacts.resolve())
output = artifacts / ("test_display_control.exe" if os.name == "nt" else "test_display_control")
command = [gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(artifacts),
           str(project / "tests/test_display_control.c"), "-o", str(output)]
build = subprocess.run(command, env=env, capture_output=True, text=True)
log = build.stdout + build.stderr
result = build.returncode
if not result:
    run = subprocess.run([str(output)], env=env, capture_output=True, text=True)
    log += run.stdout + run.stderr
    result = run.returncode
(artifacts / "result.log").write_text(log + f"EXIT: {result}\n", encoding="utf-8")
print(log)
raise SystemExit(result)
