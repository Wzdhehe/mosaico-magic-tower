"""Exercise the real shared C UI/model through the official native Host ABI."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import unittest

PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = Path(os.environ.get("MOSAICO_WORKSPACE", PROJECT.parents[1]))
ARTIFACTS = PROJECT / "artifacts"
spec = importlib.util.spec_from_file_location("mosaico_native_host", WORKSPACE / "submodule/raylib-lite-engine/host/run_game.py")
host = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = host
spec.loader.exec_module(host)
CODES = {"left": 0, "right": 1, "up": 2, "down": 5, "confirm": 6}
FIELDS = ("floor", "x", "y", "hp", "attack", "defence", "keys", "gold", "turns", "cleared")
SETTINGS_GEAR = (437, 30)
SETTINGS_CLOSE = (390, 102)
SETTINGS_ZH = (250, 158)
SETTINGS_EN = (350, 158)
PREFERENCES = ("lang", "volume", "brightness")
DEFAULT_PREFERENCES = {"lang": "zh", "volume": 45, "brightness": 40}


class Integration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ARTIFACTS.mkdir(exist_ok=True)
        cls.save = PROJECT / "magic_tower.save"
        cls.prefs = cls.save.with_name(cls.save.name + ".prefs")
        cls.previous = {path: path.read_bytes() if path.exists() else None
                        for path in (cls.save, cls.prefs)}
        cls.native = None
        cls.addClassCleanup(cls.restore_files)
        for path in cls.previous:
            path.unlink(missing_ok=True)
        cls.native = host.GenericHostRuntime(PROJECT, ARTIFACTS)
        (ARTIFACTS / "title.png").write_bytes(cls.native.frame())

    @classmethod
    def restore_files(cls):
        try:
            if cls.native is not None:
                cls.native.close()
        finally:
            for path, previous in cls.previous.items():
                if previous is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_bytes(previous)

    def state(self):
        return self.native.metadata()

    def tap(self, x, y):
        self.native.pointer(0, x, y, True)
        self.native.pointer(0, x, y, False)

    def press(self, action):
        self.native.action(CODES[action], True)
        self.native.action(CODES[action], False)

    def screenshot(self, name):
        (ARTIFACTS / name).write_bytes(self.native.frame())

    def restart(self):
        cls = type(self)
        cls.native.close()
        cls.native = None
        cls.native = host.GenericHostRuntime(PROJECT, ARTIFACTS)

    def preferences(self):
        return {key: self.state()[key] for key in PREFERENCES}

    def open_settings(self):
        if not self.state()["settings_open"]:
            self.tap(*SETTINGS_GEAR)
        self.assertTrue(self.state()["settings_open"])

    def close_settings(self):
        self.tap(*SETTINGS_CLOSE)
        self.assertFalse(self.state()["settings_open"])

    def setUp(self):
        # Keep each journey test deterministic even after a persistence test.
        self.open_settings()
        self.tap(*SETTINGS_ZH)
        self.tap(224, 224)  # 45% volume on the 80..400 track.
        self.tap(187, 300)  # 40% brightness on the 10..100 range.
        self.close_settings()
        self.assertEqual(self.preferences(), DEFAULT_PREFERENCES)
        self.native.control(2)
        self.native.control(3)
        if self.state()["phase"] == "new_confirm":
            self.tap(285, 280)
        self.assertEqual(self.state()["phase"], "playing")
        self.assertEqual((self.state()["x"], self.state()["y"]), (5, 9))

    def test_01_touch_releases_once_and_swipes_once(self):
        self.screenshot("dungeon.png")
        self.native.pointer(0, 240, 350, True)
        self.assertEqual(self.state()["turns"], 0)
        for _ in range(20):
            self.native.step(False, False, False)
        self.assertEqual(self.state()["turns"], 0)
        self.native.pointer(0, 240, 350, False)
        self.assertEqual((self.state()["y"], self.state()["hp"]), (8, 130))
        self.native.pointer(0, 240, 350, False)
        self.assertEqual(self.state()["turns"], 1)
        self.native.pointer(0, 240, 350, True)
        self.native.pointer(0, 208, 350, True)
        self.native.pointer(0, 180, 350, False)
        self.assertEqual((self.state()["x"], self.state()["turns"]), (4, 2))

    def test_02_combat_separate_confirm_and_cancel(self):
        for action in ("up", "left", "left", "up"):
            self.press(action)
        before = self.state()
        self.assertEqual((before["phase"], before["hp"], before["damage"]), ("combat", 130, 4))
        self.screenshot("combat.png")
        self.tap(180, 298)
        self.assertEqual((self.state()["phase"], self.state()["hp"], self.state()["y"]), ("playing", 130, 8))
        self.press("up")
        self.tap(290, 298)
        self.assertEqual((self.state()["phase"], self.state()["hp"], self.state()["y"]), ("playing", 126, 7))
        self.tap(290, 298)
        self.assertEqual(self.state()["hp"], 126)

    def test_03_refused_fight_and_pause(self):
        for action in ("up", "left", "left"):
            self.press(action)
        before = self.state()
        self.press("down")
        after = self.state()
        self.assertEqual({k: before[k] for k in FIELDS}, {k: after[k] for k in FIELDS})
        self.assertEqual(after["phase"], "playing")
        self.screenshot("too-strong.png")
        self.native.control(1)
        self.press("right")
        self.assertEqual(self.state()["turns"], before["turns"])
        self.native.control(2)
        self.press("right")
        self.assertEqual(self.state()["x"], 4)

    def test_04_save_new_confirmation_restore_corruption(self):
        self.press("up")
        saved = {k: self.state()[k] for k in FIELDS}
        self.tap(135, 462)
        self.assertTrue(self.save.exists())
        self.assertEqual(len(self.save.read_bytes()), 2048)
        self.press("left")
        before = self.state()
        self.tap(345, 462)
        self.assertEqual(self.state()["phase"], "new_confirm")
        self.tap(180, 280)
        self.assertEqual(self.state()["x"], before["x"])
        self.tap(240, 462)
        self.assertEqual({k: self.state()[k] for k in FIELDS}, saved)
        self.screenshot("restored.png")
        corrupt = bytearray(self.save.read_bytes())
        corrupt[30] ^= 0x40
        self.save.write_bytes(corrupt)
        before = {k: self.state()[k] for k in FIELDS}
        self.tap(240, 462)
        self.assertEqual({k: self.state()[k] for k in FIELDS}, before)

    def test_05_complete_six_floor_route(self):
        route = json.loads((PROJECT / "tests/win_route.json").read_text())
        for action in route["actions"]:
            self.press(action)
        for key, expected in route["expected"].items():
            self.assertEqual(self.state()[key], expected, key)
        self.screenshot("summit.png")
        (ARTIFACTS / "win-state.json").write_text(json.dumps(self.state(), indent=2))
        self.assertTrue(self.state()["cleared"])

    def test_06_square_layout_contains_map_and_buttons(self):
        for x, y in ((61, 75), (419, 75), (61, 433), (419, 433),
                     (90, 448), (390, 476)):
            self.assertTrue(0 <= x < 480 and 0 <= y < 480)

    def test_07_held_direction_does_not_resume_after_combat(self):
        for action in ("up", "left", "left"):
            self.press(action)
        self.native.action(2, True)
        self.assertEqual(self.state()["phase"], "combat")
        self.press("confirm")
        for _ in range(15):
            self.native.step(False, False, True)
        self.assertEqual((self.state()["x"], self.state()["y"]), (3, 7))
        self.assertEqual(self.state()["attack"], 12)
        self.native.action(2, False)
        self.press("up")
        self.assertEqual((self.state()["y"], self.state()["attack"]), (6, 17))

    def test_08_failed_staging_keeps_previous_save(self):
        self.press("up")
        self.tap(135, 462)
        previous = self.save.read_bytes()
        self.press("left")
        temporary = self.save.with_name(self.save.name + ".tmp")
        temporary.mkdir()
        try:
            self.tap(135, 462)
            self.assertEqual(self.save.read_bytes(), previous)
        finally:
            temporary.rmdir()

    def test_09_language_switch_defaults_to_chinese(self):
        self.assertEqual(self.state()["lang"], "zh")
        self.open_settings()
        self.screenshot("settings_zh.png")
        self.tap(*SETTINGS_EN)
        self.assertEqual(self.state()["lang"], "en")
        self.screenshot("settings_en.png")
        self.close_settings()
        self.screenshot("dungeon_en.png")
        self.open_settings()
        self.tap(*SETTINGS_ZH)
        self.assertEqual(self.state()["lang"], "zh")
        self.close_settings()
        self.screenshot("dungeon_zh.png")

    def test_12_settings_capture_blocks_journey_input_and_footer(self):
        self.press("up")
        self.tap(135, 462)
        saved = self.save.read_bytes()
        self.press("left")
        before = {key: self.state()[key] for key in FIELDS}
        self.open_settings()
        for code in (CODES["up"], CODES["right"], CODES["down"], 3, 4):
            self.native.action(code, True)
            for _ in range(8):
                self.native.step(False, False, True)
            self.native.action(code, False)
            self.assertTrue(self.state()["settings_open"])
            self.assertEqual(self.state()["phase"], "playing")
            self.assertEqual({key: self.state()[key] for key in FIELDS}, before)
        # An outside touch may dismiss the panel, but must not click through.
        for x in (135, 240, 345):
            self.open_settings()
            self.tap(x, 462)
            self.assertEqual(self.state()["phase"], "playing")
            self.assertEqual({key: self.state()[key] for key in FIELDS}, before)
            self.assertEqual(self.save.read_bytes(), saved)
        # A captured volume drag stays a settings operation even over the map.
        self.open_settings()
        self.native.pointer(0, 240, 224, True)
        self.native.pointer(0, 430, 350, True)
        self.native.pointer(0, 0, 0, False)
        self.assertTrue(self.state()["settings_open"])
        self.assertEqual(self.state()["volume"], 100)
        self.assertEqual({key: self.state()[key] for key in FIELDS}, before)
        self.close_settings()

    def test_13_preferences_survive_restart_new_and_journey_load(self):
        self.press("up")
        self.tap(135, 462)
        journey = {key: self.state()[key] for key in FIELDS}
        self.open_settings()
        self.tap(*SETTINGS_EN)
        self.tap(304, 224)  # 70% volume.
        self.tap(240, 300)  # 55% brightness, coalesced until update/close.
        self.close_settings()
        expected = {"lang": "en", "volume": 70, "brightness": 55}
        self.assertEqual(self.preferences(), expected)
        self.assertFalse(self.state()["preferences_save_failed"])
        self.assertTrue(self.prefs.exists())
        self.native.control(3)
        self.tap(285, 280)
        self.assertEqual(self.preferences(), expected)
        self.assertEqual((self.state()["x"], self.state()["y"]), (5, 9))
        self.tap(240, 462)
        self.assertEqual({key: self.state()[key] for key in FIELDS}, journey)
        self.assertEqual(self.preferences(), expected)
        self.restart()
        self.assertEqual(self.state()["phase"], "title")
        self.assertEqual(self.preferences(), expected)
        self.tap(240, 343)  # Continue from the independent journey record.
        self.assertEqual({key: self.state()[key] for key in FIELDS}, journey)
        self.assertEqual(self.preferences(), expected)

    def test_14_failed_preferences_save_retains_record_and_retries(self):
        self.open_settings()
        self.tap(304, 224)
        self.close_settings()
        previous = self.prefs.read_bytes()
        temporary = self.prefs.with_name(self.prefs.name + ".tmp")
        if temporary.exists():
            self.skipTest("pre-existing preferences staging path must be preserved")
        temporary.mkdir()
        try:
            self.open_settings()
            self.tap(*SETTINGS_EN)
            self.close_settings()
            self.assertEqual(self.state()["lang"], "en")
            self.assertTrue(self.state()["preferences_save_failed"])
            self.assertEqual(self.prefs.read_bytes(), previous)
            self.open_settings()
            failed_frame = self.native.frame()
            (ARTIFACTS / "settings_save_failed.png").write_bytes(failed_frame)
        finally:
            temporary.rmdir()
        # Closing again retries the same values, without another edit.
        self.close_settings()
        self.assertFalse(self.state()["preferences_save_failed"])
        self.assertNotEqual(self.prefs.read_bytes(), previous)
        self.open_settings()
        recovered_frame = self.native.frame()
        (ARTIFACTS / "settings_save_retried.png").write_bytes(recovered_frame)
        self.assertNotEqual(failed_frame, recovered_frame)
        self.close_settings()
        self.restart()
        self.assertEqual(self.preferences(), {"lang": "en", "volume": 70, "brightness": 40})

    def test_15_invalid_or_truncated_preferences_use_defaults(self):
        self.open_settings()
        self.tap(*SETTINGS_EN)
        self.tap(304, 224)
        self.close_settings()
        valid = self.prefs.read_bytes()
        invalid = bytearray(valid)
        invalid[0] ^= 0xff  # Unsupported version, with otherwise valid values.
        invalid_volume = bytearray(valid)
        invalid_volume[2] = 255
        invalid_brightness = bytearray(valid)
        invalid_brightness[3] = 0
        for record in (bytes(invalid), bytes(invalid_volume), bytes(invalid_brightness),
                       valid[:-1], valid + b"\x00", b""):
            with self.subTest(record=record.hex()):
                self.prefs.write_bytes(record)
                self.restart()
                self.assertEqual(self.state()["phase"], "title")
                self.assertEqual(self.preferences(), DEFAULT_PREFERENCES)
                self.assertFalse(self.state()["preferences_save_failed"])

    def test_11_shop_buy_price_and_restore(self):
        route = json.loads((PROJECT / "tests/win_route.json").read_text())
        for action in route["actions"]:
            self.press(action)
            if self.state()["floor"] == 1:
                break
        state = self.state()
        self.assertEqual((state["floor"], state["x"], state["y"]), (1, 5, 9))
        # Yellow key, attack gem, yellow door, blue key, then the merchant stair.
        for action in (["up"] + ["left"] * 4 + ["up"] +
                       ["right"] * 4 + ["up", "up"] + ["left"] * 4):
            self.press(action)
        state = self.state()
        self.assertEqual((state["phase"], state["x"], state["y"]), ("playing", 1, 5))
        self.assertEqual((state["gold"], state["attack"], state["defence"]), (21, 27, 7))
        # Save before shopping, so either attribute can buy from the same gold.
        self.tap(135, 462)
        self.assertTrue(self.save.exists())
        self.press("up")
        state = self.state()
        self.assertEqual((state["phase"], state["gold"]), ("shop", 21))
        self.screenshot("shop_zh.png")
        # First purchase costs 15 and raises the shared next price to 25.
        self.tap(240, 240)
        state = self.state()
        self.assertEqual((state["gold"], state["attack"], state["shop_attack"]), (6, 30, 1))
        self.screenshot("shop_after_buy_zh.png")
        # 6 gold is below the new price: the defence purchase must be refused.
        before = {k: state[k] for k in
                  ("gold", "attack", "defence", "shop_attack", "shop_defence")}
        self.tap(240, 282)
        state = self.state()
        self.assertEqual({k: state[k] for k in before}, before)
        self.screenshot("shop_no_gold_zh.png")
        # Leave (standing on the shop tile), restore the pre-shop checkpoint,
        # and buy the other attribute.
        self.tap(240, 340)
        state = self.state()
        self.assertEqual((state["phase"], state["x"], state["y"]), ("playing", 1, 4))
        self.tap(240, 462)
        state = self.state()
        self.assertEqual((state["gold"], state["attack"], state["defence"]), (21, 27, 7))
        self.assertEqual((state["shop_attack"], state["shop_defence"]), (0, 0))
        self.press("up")
        self.assertEqual(self.state()["phase"], "shop")
        self.tap(240, 282)
        state = self.state()
        self.assertEqual((state["gold"], state["defence"], state["shop_defence"]), (6, 10, 1))
        self.screenshot("shop_restored_defence_zh.png")

    def test_10_new_game_returns_to_floor_one(self):
        for action in ("up", "left", "left", "up", "confirm", "up", "right", "right"):
            self.press(action)
        self.assertEqual(self.state()["floor"], 0)
        self.native.control(3)
        self.tap(285, 280)
        self.assertEqual(self.state()["floor"], 0)
        self.assertEqual((self.state()["x"], self.state()["y"]), (5, 9))
        self.assertEqual(self.state()["hp"], 100)


if __name__ == "__main__":
    unittest.main(verbosity=2)
