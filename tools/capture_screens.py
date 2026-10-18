#!/usr/bin/env python3
"""Capture real C-Host screenshots for the docs and the campaign review.

Run from the official workspace copy of the project: the shared Host engine is
resolved relative to that workspace. Every image comes from the project's own C
renderer through the official Host ABI; this script only drives the game and
crops the 11x11 dungeon out of those frames. It never reimplements rendering or
map logic.
"""
import importlib.util
import io
import json
from pathlib import Path
import re
import sys

from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]
ARTIFACTS = PROJECT / "artifacts"
IMAGES = PROJECT / "docs" / "images"
HOST = WORKSPACE / "submodule/raylib-lite-engine/host/run_game.py"
CODES = {"left": 0, "right": 1, "up": 2, "down": 5, "confirm": 6}
# tower_status_t, in declaration order.
NEED_KEY, DOOR, TALK = 3, 5, 14
LANG_CHIP = (437, 18)
START_BUTTON = (240, 299)
NEW_GAME_YES = (285, 280)
SHOP_ATTACK = (240, 240)
SHOP_LEAVE = (240, 340)

spec = importlib.util.spec_from_file_location("mosaico_native_host", HOST)
host = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = host
spec.loader.exec_module(host)


def press(runtime, action):
    runtime.action(CODES[action], True)
    runtime.action(CODES[action], False)


def tap(runtime, x, y):
    runtime.pointer(0, x, y, True)
    runtime.pointer(0, x, y, False)


def frame(runtime, name):
    data = runtime.frame()
    (ARTIFACTS / name).write_bytes(data)
    return data


def map_geometry():
    config = (PROJECT / "main" / "game_config.h").read_text(encoding="utf-8")
    values = {}
    for name in ("GAME_MAP_X", "GAME_MAP_Y", "GAME_TILE"):
        match = re.search(rf"{name}\s*=\s*(\d+)", config)
        if not match:
            raise RuntimeError(f"missing {name} in game_config.h")
        values[name] = int(match.group(1))
    return values["GAME_MAP_X"], values["GAME_MAP_Y"], values["GAME_TILE"]


def floor_crop(data, geometry, target):
    left, top, tile = geometry
    image = Image.open(io.BytesIO(data))
    crop = image.crop((left, top, left + tile * 11, top + tile * 11))
    crop = crop.resize((tile * 22, tile * 22), Image.Resampling.NEAREST)
    crop.save(target)


def start_journey(runtime):
    """Reach floor one from the title screen with the default language."""
    assert runtime.metadata()["phase"] == "title"
    tap(runtime, *START_BUTTON)
    state = runtime.metadata()
    assert state["phase"] == "playing", state
    assert (state["floor"], state["x"], state["y"]) == (0, 5, 9), state
    return state


def new_journey(runtime):
    runtime.control(3)
    if runtime.metadata()["phase"] == "new_confirm":
        tap(runtime, *NEW_GAME_YES)
    state = runtime.metadata()
    assert (state["phase"], state["floor"], state["x"], state["y"]) == \
        ("playing", 0, 5, 9), state
    return state


def capture_blocked_key(runtime, geometry):
    """Bump the floor-one yellow door without its key."""
    for _ in range(3):
        press(runtime, "up")
    state = runtime.metadata()
    assert state["status"] == NEED_KEY, state
    # Two steps north, then the locked floor-one yellow door refuses the third.
    assert state["floor"] == 0 and (state["x"], state["y"]) == (5, 7), state
    data = frame(runtime, "blocked_key_zh.png")
    floor_crop(data, geometry, ARTIFACTS / "blocked_key.png")
    return state


def campaign_captures():
    geometry = map_geometry()
    runtime = host.GenericHostRuntime(PROJECT, ARTIFACTS)
    try:
        (IMAGES).mkdir(parents=True, exist_ok=True)
        (ARTIFACTS / "title.png").write_bytes(runtime.frame())
        (IMAGES / "title.png").write_bytes((ARTIFACTS / "title.png").read_bytes())
        start_journey(runtime)
        dungeon = frame(runtime, "dungeon_zh.png")
        (IMAGES / "gameplay.png").write_bytes(dungeon)
        (IMAGES / "dungeon.png").write_bytes(dungeon)

        blocked = capture_blocked_key(runtime, geometry)
        new_journey(runtime)

        tap(runtime, *LANG_CHIP)
        assert runtime.metadata()["lang"] == "en"
        frame(runtime, "dungeon_en.png")
        tap(runtime, *LANG_CHIP)
        assert runtime.metadata()["lang"] == "zh"

        floors, dialogs = set(), set()
        saw_combat = saw_shop = saw_door = saw_gain = False
        previous = runtime.metadata()
        final = None

        def floor_frame():
            state = runtime.metadata()
            floor = int(state["floor"])
            floors.add(floor)
            data = frame(runtime, f"floor{floor + 1}_full_zh.png")
            floor_crop(data, geometry, IMAGES / f"floor{floor + 1}.png")
            if state["cleared"]:
                (ARTIFACTS / "campaign_cleared.png").write_bytes(data)

        floor_frame()
        route = json.loads((PROJECT / "tests" / "win_route.json").read_text())
        for action in route["actions"]:
            press(runtime, action)
            state = runtime.metadata()
            floor = int(state["floor"])
            if floor not in floors:
                floor_frame()
            if state["status"] == TALK and floor not in dialogs:
                dialogs.add(floor)
                data = frame(runtime, f"npc_floor{floor + 1}_zh.png")
                if floor == 0:
                    (IMAGES / "npc-dialogue.png").write_bytes(data)
            if state["status"] == DOOR:
                saw_door = True
            if state["phase"] == "combat" and not saw_combat:
                saw_combat = True
                frame(runtime, "combat_zh.png")
            if state["phase"] == "shop" and not saw_shop:
                saw_shop = True
                frame(runtime, "shop_zh.png")
            gained = (
                state["attack"] > previous["attack"] or
                state["defence"] > previous["defence"] or
                state["gold"] > previous["gold"] or
                sum(state["keys"]) > sum(previous["keys"]) or
                state["hp"] > previous["hp"])
            if gained and not saw_gain:
                saw_gain = True
                data = frame(runtime, "pickup-gain.png")
                (IMAGES / "pickup-gain.png").write_bytes(data)
            if state["cleared"]:
                final = frame(runtime, "victory_zh.png")
                (IMAGES / "summit.png").write_bytes(final)
                break
            previous = state
        else:
            raise RuntimeError("win route did not clear the tower")

        assert final is not None
        assert floors == set(range(10)), sorted(floors)
        assert dialogs, "no NPC dialogue was reached"
        assert saw_combat and saw_door and saw_gain, (saw_combat, saw_door, saw_gain)
        (ARTIFACTS / "campaign-state.json").write_text(
            json.dumps(runtime.metadata(), indent=2), encoding="utf-8")
        print("blocked-key status:", blocked["status"], "at", blocked["x"], blocked["y"])
        print("floors:", sorted(f + 1 for f in floors))
        print("dialogues:", sorted(f + 1 for f in dialogs))
        print("combat:", saw_combat, "door:", saw_door, "shop in route:", saw_shop)
    finally:
        runtime.close()


def shop_captures():
    """Walk the second-floor merchant path and photograph the price ladder."""
    runtime = host.GenericHostRuntime(PROJECT, ARTIFACTS)
    try:
        start_journey(runtime)
        route = json.loads((PROJECT / "tests" / "win_route.json").read_text())
        for action in route["actions"]:
            press(runtime, action)
            if runtime.metadata()["floor"] == 1:
                break
        assert runtime.metadata()["floor"] == 1
        # Same accepted approach as tests/test_integration.py: reach (1, 5)
        # beside the merchant, then step onto the shop tile.
        for action in (["up"] + ["left"] * 4 + ["up"] +
                       ["right"] * 4 + ["up", "up"] + ["left"] * 4):
            press(runtime, action)
        state = runtime.metadata()
        assert (state["phase"], state["x"], state["y"]) == ("playing", 1, 5), state
        before = (state["gold"], state["shop_attack"], state["shop_defence"])
        press(runtime, "up")
        state = runtime.metadata()
        assert (state["phase"], state["gold"]) == ("shop", 21), state
        data = frame(runtime, "shop_zh.png")
        (IMAGES / "shop.png").write_bytes(data)
        tap(runtime, *SHOP_ATTACK)
        state = runtime.metadata()
        assert (state["gold"], state["attack"], state["shop_attack"]) == (6, 30, 1), state
        frame(runtime, "shop_after_buy_zh.png")
        tap(runtime, *SHOP_LEAVE)
        assert runtime.metadata()["phase"] == "playing"
        print("shop gold/price before:", before, "after purchase:", state["gold"],
              state["shop_attack"])
    finally:
        runtime.close()


if __name__ == "__main__":
    campaign_captures()
    shop_captures()
    print("captured", len(list(IMAGES.glob("*.png"))), "doc images")
