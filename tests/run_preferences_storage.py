"""Validate the same preference codec and file adapter used by native Host."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
artifacts=ROOT/"artifacts"
artifacts.mkdir(exist_ok=True)
cc=os.environ.get("CC") or shutil.which("gcc") or "gcc"
with tempfile.TemporaryDirectory(prefix="prefs-",dir=artifacts) as directory:
    build=Path(directory)
    blocked=build/"blocked.prefs"
    blocked.write_bytes(bytes((1,0,45,40)))
    (build/"blocked.prefs.tmp").mkdir()
    executable=build/("test.exe" if os.name=="nt" else "test")
    command=[cc,"-std=c11","-Wall","-Wextra","-Werror","-pedantic",
             "-I",str(ROOT/"main"),str(ROOT/"tests/test_preferences_storage.c"),
             str(ROOT/"main/game_preferences.c"),str(ROOT/"main/host_storage.c"),
             "-o",str(executable)]
    result=subprocess.run(command,capture_output=True,text=True)
    log="BUILD:\n"+result.stdout+result.stderr
    if not result.returncode:
        result=subprocess.run([str(executable),str(build/"chosen.prefs"),str(blocked)],
                              capture_output=True,text=True)
        log+="RUN:\n"+result.stdout+result.stderr
    log+=f"EXIT: {result.returncode}\n"
    (artifacts/"preferences-storage.log").write_text(log,encoding="utf-8")
    print(log)
    raise SystemExit(bool(result.returncode))
