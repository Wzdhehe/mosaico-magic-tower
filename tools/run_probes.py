#!/usr/bin/env python3
"""Build and run the render-cost and model-defect probes for this project.

Both probes are host-only: they compile the shipping C sources with a
counting stub in place of the Raylib-Lite / GSP graphics backend, so the
numbers they print describe the code that actually runs on the device.

    python tools/run_probes.py            # build + run both probes
    python tools/run_probes.py --list     # show the files involved

Nothing here touches the device or the ESP-IDF build.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
PROBE = PROJECT / "tools" / "render_probe"
OUT = PROJECT / "artifacts" / "probes"

PROBES = {
    "render": {
        "c": [PROBE / "measure_render.c",
              PROJECT / "main" / "game_view.c",
              PROJECT / "main" / "game.c",
              PROJECT / "main" / "tower.c"],
        "include": [PROBE, PROJECT / "main"],
        "libs": ["-lm"],
        "doc": "per-frame draw-call and pixel-write cost, plus a pixel diff "
               "between consecutive frames",
    },
    "defects": {
        "c": [PROBE / "verify_model_defects.c",
              PROJECT / "main" / "tower.c"],
        "include": [PROJECT / "main"],
        "libs": [],
        "doc": "save-validation and fail-state checks against the portable model",
    },
}


def build(name, spec, cc):
    OUT.mkdir(parents=True, exist_ok=True)
    binary = OUT / f"probe_{name}"
    cmd = [cc, "-std=c11", "-O2", "-g"]
    for inc in spec["include"]:
        cmd += ["-I", str(inc)]
    cmd += [str(c) for c in spec["c"]]
    cmd += ["-o", str(binary)] + spec["libs"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode:
        print(f"[{name}] BUILD FAILED")
        print(proc.stdout)
        print(proc.stderr)
        return None
    return binary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="list the probes and exit")
    ap.add_argument("--only", choices=sorted(PROBES), help="run a single probe")
    args = ap.parse_args()

    if args.list:
        for name, spec in PROBES.items():
            print(f"{name:8s} {spec['doc']}")
            for c in spec["c"]:
                print(f"         {c.relative_to(PROJECT)}")
        return 0

    cc = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not cc:
        print("no C compiler found; set CC", file=sys.stderr)
        return 1

    failures = 0
    for name, spec in PROBES.items():
        if args.only and name != args.only:
            continue
        binary = build(name, spec, cc)
        if not binary:
            failures += 1
            continue
        print("=" * 78)
        print(f"PROBE: {name}  --  {spec['doc']}")
        print("=" * 78)
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        print(run.stdout)
        if run.stderr:
            print(run.stderr, file=sys.stderr)
        if run.returncode == 3:
            # The probe ran successfully and found something worth reporting.
            print(f"[{name}] findings present (exit 3) -- see the report above")
        elif run.returncode:
            print(f"[{name}] probe failed (exit {run.returncode})")
            failures += 1
        print()
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
