#!/usr/bin/env python3
"""Run timing/input C regression plus the Unicode font order prerequisite.

Usage: python tests/run_game_timing.py
The C game model is compiled without the ESP-IDF toolchain.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def check_glyph_order():
    text=(ROOT/"main"/"ui_font.h").read_text(encoding="utf-8")
    glyphs=[int(cp,16) for cp in re.findall(r"\{0x([0-9a-fA-F]+),\s*\{",text)]
    assert len(glyphs)>=1, "font header has no glyphs"
    assert all(a<b for a,b in zip(glyphs,glyphs[1:])), (
        "glyph_for binary search requires a strictly sorted, unique table")
    print(f"PASS: {len(glyphs)} glyph codepoints strictly increasing")


def main():
    check_glyph_order()
    cc=os.environ.get("CC") or shutil.which("gcc") or "gcc"
    with tempfile.TemporaryDirectory(prefix="magic-tower-timing-") as td:
        binary=Path(td)/("timing.exe" if os.name=="nt" else "timing")
        command=[cc,"-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                 "-I",str(ROOT/"main"),
                 str(ROOT/"tests"/"test_game_timing.c"),
                 str(ROOT/"main"/"game.c"),
                 str(ROOT/"main"/"tower.c"),
                 "-o",str(binary)]
        subprocess.run(command,check=True)
        subprocess.run([str(binary)],check=True)


if __name__=="__main__":
    main()
