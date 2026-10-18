"""Generate four fork strategies and verify their actions in the real C model.

Uses only the standard library and a C11 compiler, like run_tests.py.
Generated headers and binaries stay in a temporary artifacts directory.
"""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
ARTIFACTS = PROJECT / "artifacts"
spec = importlib.util.spec_from_file_location("magic_tower_route", PROJECT / "tools/magic_tower_route.py")
route = importlib.util.module_from_spec(spec)
spec.loader.exec_module(route)


def strategy_header():
    lines = ["/* Generated for this test invocation. */"]
    records = []
    for first in ("m", "g"):
        for second in ("g", "w"):
            game = route.plan_variant({6: first, 9: second})
            state = game.summary()
            assert state["cleared"] and state["hp"] > 0
            name = f"strategy_{first}_{second}"
            actions = ",".join(str(route.ACTION_CODE[action]) for action in game.actions)
            lines.append(f"static const signed char {name}[] = {{{actions}}};")
            fields = ",".join(str(state[key]) for key in
                              ("floor", "x", "y", "hp", "attack", "defence"))
            keys = ",".join(str(value) for value in state["keys"])
            records.append(
                f'    {{"6F {first} / 9F {second}", {name}, sizeof {name}, '
                f'{fields}, {{{keys}}}, {state["gold"]}, {state["turns"]}, '
                f'true, {str(first == "m").lower()}, {str(second == "g").lower()}}},')
    lines.extend(["static const strategy_t STRATEGIES[] = {", *records, "};", ""])
    return "\n".join(lines)


def main():
    ARTIFACTS.mkdir(exist_ok=True)
    compiler = os.environ.get("CC") or shutil.which("gcc") or "gcc"
    gcc = Path(shutil.which(compiler) or compiler)
    if not gcc.exists():
        raise SystemExit(f"gcc not found: {gcc}. Set CC or add gcc to PATH.")
    env = os.environ.copy()
    if gcc.parent != Path("."):
        env["PATH"] = str(gcc.parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="strategy-routes-", dir=ARTIFACTS) as directory:
        build_dir = Path(directory)
        (build_dir / "strategy_routes.h").write_text(strategy_header(), encoding="utf-8")
        executable = build_dir / "test_strategy_routes.exe"
        command = [str(gcc), "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-g",
                   "-I", str(PROJECT / "main"), "-I", str(build_dir),
                   str(PROJECT / "tests/test_strategy_routes.c"), str(PROJECT / "main/tower.c"),
                   "-o", str(executable)]
        build = subprocess.run(command, capture_output=True, text=True, env=env)
        log = "COMPILE: " + " ".join(command) + "\n" + build.stdout + build.stderr
        result = build.returncode
        if result == 0:
            run = subprocess.run([str(executable)], capture_output=True, text=True, env=env)
            log += "RUN:\n" + run.stdout + run.stderr
            result = run.returncode
        log += f"EXIT: {result}\n"
        (ARTIFACTS / "strategy-routes.log").write_text(log, encoding="utf-8")
        print(log)
        return int(result != 0)


if __name__ == "__main__":
    raise SystemExit(main())
