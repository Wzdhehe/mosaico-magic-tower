#!/usr/bin/env python3
"""Capture title/dungeon/combat screenshots in both languages."""
import importlib.util
import os
from pathlib import Path
import sys

PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]
ARTIFACTS = PROJECT / "artifacts"
os.environ["PATH"] = "D:/espbulid/mingw64/bin" + os.pathsep + os.environ.get("PATH", "")
HOST = WORKSPACE / "submodule/raylib-lite-engine/host/run_game.py"
spec = importlib.util.spec_from_file_location("mosaico_native_host", HOST)
host = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = host
spec.loader.exec_module(host)


def press(rt, code):
    rt.action(code, True)
    rt.action(code, False)


def frame(rt, name):
    (ARTIFACTS / name).write_bytes(rt.frame())


def main():
    rt = host.GenericHostRuntime(PROJECT, ARTIFACTS)
    frame(rt, "title_zh.png")
    rt.pointer(0, 437, 18, True)
    rt.pointer(0, 437, 18, False)
    frame(rt, "title_en.png")
    rt.pointer(0, 437, 18, True)
    rt.pointer(0, 437, 18, False)
    rt.pointer(0, 240, 299, True)
    rt.pointer(0, 240, 299, False)
    for _ in range(3):
        rt.step(False, False, False)
    frame(rt, "dungeon_zh.png")
    press(rt, 2)      # up to potion
    press(rt, 0); press(rt, 0)
    press(rt, 2)      # into slime -> combat
    frame(rt, "combat_zh.png")
    press(rt, 6)      # confirm
    for _ in range(2):
        rt.step(False, False, False)
    rt.pointer(0, 437, 18, True)
    rt.pointer(0, 437, 18, False)
    frame(rt, "dungeon_en.png")
    rt.close()
    print("captured")


if __name__ == "__main__":
    main()
