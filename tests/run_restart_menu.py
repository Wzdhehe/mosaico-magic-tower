#!/usr/bin/env python3
"""Compile the restart flow against the real game and tower models."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc = os.environ.get("CC") or shutil.which("gcc") or "gcc"
    with tempfile.TemporaryDirectory(prefix="restart-menu-") as td:
        binary = Path(td) / ("restart.exe" if os.name == "nt" else "restart")
        subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                        "-I", str(ROOT / "main"),
                        str(ROOT / "tests/test_restart_menu.c"),
                        str(ROOT / "main/game.c"), str(ROOT / "main/tower.c"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
