"""Regression tests for decision-focused map auditing.

These tests verify semantics, not arbitrary monster/door density quotas.
"""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "tower_audit", ROOT / "tools" / "audit_tower_decisions.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DecisionAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "main" / "tower.c").read_text(encoding="utf-8")
        cls.maps = module.load_maps(cls.src)
        cls.enemies = module.load_enemies(cls.src)
        cls.report = module.audit(cls.src)

    def test_real_enemy_formula(self):
        self.assertEqual(module.loss(self.enemies["g"], 62, 28), 6)
        self.assertEqual(module.loss(self.enemies["w"], 62, 28), 48)
        self.assertEqual(module.loss(self.enemies["w"], 62, 31), 30)

    def test_required_progression(self):
        self.assertTrue(all(module.contract(self.report).values()))

    def test_floor_nine_costs_and_pickups(self):
        golem = self.report["f9_golem_path"]
        wraith = self.report["f9_wraith_path"]
        self.assertEqual((golem["loss_at_fixed_stats"], golem["steps"]), (6, 14))
        self.assertEqual((wraith["loss_at_fixed_stats"], wraith["steps"]), (48, 20))
        self.assertEqual(self.report["f9_golem_replay"]["hp"], 695)
        self.assertEqual(self.report["f9_wraith_replay"]["hp"], 751)
        self.assertEqual(self.report["f9_wraith_replay"]["defence"], 31)
        self.assertTrue(self.report["f9_same_accessible_loot"])

    def test_empty_enemy_special_floor_is_not_a_bug(self):
        self.assertTrue(self.report["f4_quiet"])
        self.assertEqual(self.report["distinct_walls_f3_plus"], 8)

    def test_broken_fork_is_caught(self):
        mutated = [list(row) for row in self.maps[8]]
        mutated[7][1] = "#"  # remove the wraith route
        altered = ["".join(row) for row in mutated]
        self.assertIsNone(
            module.shortest_guard_path(altered, self.enemies, 62, 28, blocked="g"))
        self.assertIsNotNone(
            module.shortest_guard_path(altered, self.enemies, 62, 28, blocked="w"))


if __name__ == "__main__":
    unittest.main()
