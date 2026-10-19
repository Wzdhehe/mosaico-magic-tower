#!/usr/bin/env python3
"""Compile the actual game/tower models against inert camera fixtures."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc = os.environ.get("CC") or shutil.which("gcc")
    if not cc:
        candidates = [ROOT / "mingw64" / "bin" / "gcc.exe",
                      ROOT.parent.parent / "mingw64" / "bin" / "gcc.exe"]
        cc = next((str(path) for path in candidates if path.is_file()), "gcc")
    env = os.environ.copy()
    env["PATH"] = str(Path(cc).resolve().parent) + os.pathsep + env.get("PATH", "")
    artifacts = ROOT / "artifacts"
    artifacts.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="camera-tests-", dir=artifacts) as td:
        binary = Path(td) / ("camera.exe" if os.name == "nt" else "camera")
        subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                        "-I", str(ROOT / "main"),
                        str(ROOT / "tests" / "test_camera_controls.c"),
                        str(ROOT / "main" / "game.c"),
                        str(ROOT / "main" / "tower.c"), "-o", str(binary)], check=True, env=env)
        subprocess.run([str(binary)], check=True, env=env)


if __name__ == "__main__":
    main()
