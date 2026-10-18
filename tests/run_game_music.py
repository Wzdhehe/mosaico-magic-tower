#!/usr/bin/env python3
"""Compile and run the portable music tests, retaining evidence in artifacts/music."""
import os
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]

def main():
    dest = ROOT / "artifacts/music"
    dest.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("CC") or shutil.which("gcc") or "gcc"
    binary = dest / ("music-tests.exe" if os.name == "nt" else "music-tests")
    command = [cc, "-std=c11", "-O2", "-Dcalloc=game_music_test_calloc", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(ROOT / "main"),
               str(ROOT / "tests/test_game_music.c"), str(ROOT / "main/game_music.c"), "-o", str(binary)]
    temporary = ROOT / "artifacts/tmp"
    temporary.mkdir(parents=True, exist_ok=True)
    build_env = os.environ.copy()
    build_env.update(TEMP=str(temporary), TMP=str(temporary))
    subprocess.run(command, check=True, env=build_env)
    result = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout, end="")
    (dest / ("green.txt" if result.returncode == 0 else "red.txt")).write_text(result.stdout, encoding="utf-8")
    raise SystemExit(result.returncode)
if __name__ == "__main__":
    main()
