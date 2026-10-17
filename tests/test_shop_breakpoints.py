"""Mathematical economy analysis regression tests; not global pathfinding."""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "audit_shop_breakpoints", ROOT / "tools" / "audit_shop_breakpoints.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class ShopBreakpointTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = (ROOT / "main" / "tower.c").read_text(encoding="utf-8")
        header = (ROOT / "main" / "tower.h").read_text(encoding="utf-8")
        route = json.loads(
            (ROOT / "tests" / "win_route.json").read_text(encoding="utf-8"))
        cls.result = audit.analyze(src, header, route)

    def test_reference_boss_damage_and_currency(self):
        r = self.result
        self.assertEqual(r["enemy"],
                         {"hp": 700, "attack": 58, "defence": 46, "gold": 150})
        self.assertEqual(r["reference"]["hp"], 921)
        self.assertEqual(r["reference"]["gold"], 229)  # boss reward not yet owned
        self.assertEqual(r["baseline"]["boss_damage"], 546)

    def test_merchant_purchase_breakpoints(self):
        r = self.result
        self.assertEqual((r["shop_cost"], r["shop_gain"]), (15, 3))
        self.assertEqual(r["first_attack"]["boss_damage"], 504)
        self.assertEqual(r["first_defence"]["boss_damage"], 468)
        self.assertEqual(
            audit.allocation(r["enemy"], r["reference"], 15, 3, 4, 0)
            ["boss_damage"], 378)
        self.assertEqual(
            audit.allocation(r["enemy"], r["reference"], 15, 3, 2, 2)
            ["boss_damage"], 315)
        self.assertEqual(
            audit.allocation(r["enemy"], r["reference"], 15, 3, 0, 4)
            ["boss_damage"], 234)

    def test_early_boss_damage_neutralization(self):
        r = self.result
        zero = r["cheapest_zero"]
        self.assertIsNotNone(zero)
        self.assertEqual(zero["gold_spent"], 105)
        self.assertEqual((zero["atk_buys"], zero["def_buys"]), (0, 7))
        self.assertEqual(zero["boss_damage"], 0)
        self.assertEqual(zero["hp_after_boss"], 921)

    def test_no_attack_when_defence_meets_attack(self):
        boss = self.result["enemy"]
        self.assertEqual(audit.battle_damage(boss, 72, 58), 0)
        self.assertEqual(audit.battle_damage(boss, 72, 57), 26)
        self.assertIsNone(audit.battle_damage(boss, 46, 58))


if __name__ == "__main__":
    unittest.main()
