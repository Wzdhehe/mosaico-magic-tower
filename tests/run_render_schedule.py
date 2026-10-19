#!/usr/bin/env python3
"""Test operation-driven scheduling against the pinned real RGB565 engine.

Set MOSAICO_ENGINE_ROOT to the official workspace's Raylib Lite checkout.
Only the frame acquisition/presentation port is replaced by a counting spy.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def find_engine() -> Path:
    """Resolve the pinned engine without silently accepting a wrong path.

    MOSAICO_ENGINE_ROOT is authoritative when set. Otherwise try the official
    workspace layout next to this worktree, then the classic project layout.
    """
    marker = Path("host/include/raylib.h")
    configured = os.environ.get("MOSAICO_ENGINE_ROOT")
    if configured:
        engine = Path(configured)
        if not (engine / marker).is_file():
            raise SystemExit(
                f"MOSAICO_ENGINE_ROOT does not look like a Raylib Lite checkout: {engine}")
        return engine.resolve()
    candidates = [
        ROOT.parents[1] / "esp-mosaico-vibe/submodule/raylib-lite-engine",
        ROOT.parents[1] / "submodule/raylib-lite-engine",
        ROOT.parents[2] / "esp-mosaico-vibe/submodule/raylib-lite-engine",
        ROOT.parents[2] / "submodule/raylib-lite-engine",
    ]
    for candidate in candidates:
        if (candidate / marker).is_file():
            return candidate.resolve()
    raise SystemExit(
        "Raylib Lite engine not found; set MOSAICO_ENGINE_ROOT to the pinned checkout "
        f"(tried {', '.join(str(c) for c in candidates)})")


def main():
    engine = find_engine()
    cc = os.environ.get("CC") or shutil.which("gcc") or "gcc"
    sources = [ROOT / "tests/test_render_schedule.c",
               ROOT / "main/game.c", ROOT / "main/tower.c", ROOT / "main/tower_floors.c",
               ROOT / "main/game_view.c",
               ROOT / "main/game_device_render.c",
               engine / "host/host_asset_runtime.c",
               engine / "components/mosaico_game_2d/mosaico_game_2d.c",
               engine / "components/mosaico_raylib_fast/mosaico_raylib_fast.c",
               engine / "components/mosaico_game_fx/mosaico_game_fx.c",
               engine / "components/mosaico_game_tilemap/mosaico_game_tilemap.c"]
    includes = [ROOT / "main", engine / "components/mosaico_game/include",
                engine / "host/include", engine / "host",
                *[engine / "components" / component / "include" for component in (
                    "mosaico_game_assets", "mosaico_game_2d", "mosaico_raylib_fast",
                    "mosaico_game_fx", "mosaico_game_tilemap")]]
    artifacts = ROOT / "artifacts"
    artifacts.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="magic-render-") as temporary:
        # The official Host port header includes the reduced Host game API by
        # relative path. Redirect only that include to the real device API so
        # this port spy can also exercise submission-statistics recovery.
        port_header = (engine / "host/include/mosaico_raylib_port.h").read_text()
        device_api = (engine / "components/mosaico_game/include/mosaico_game.h").as_posix()
        port_header = port_header.replace('#include "mosaico_game.h"',
                                          f'#include "{device_api}"')
        (Path(temporary) / "mosaico_raylib_port.h").write_text(port_header)
        binary = Path(temporary) / ("render.exe" if os.name == "nt" else "render")
        command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                   "-DMOSAICO_HOST_SIMULATION=1", "-I", temporary, *map(str, sources)]
        for include in includes:
            command.extend(("-I", str(include)))
        command.extend(("-lm", "-o", str(binary)))
        build = subprocess.run(command, capture_output=True, text=True)
        log = "BUILD:\n" + build.stdout + build.stderr
        result = build.returncode
        if not result:
            run = subprocess.run([str(binary)], capture_output=True, text=True)
            log += "RUN:\n" + run.stdout + run.stderr
            result = run.returncode
        log += f"EXIT: {result}\n"
        (artifacts / "render-schedule.log").write_text(log, encoding="utf-8")
        print(log)
        raise SystemExit(bool(result))


if __name__ == "__main__":
    main()
