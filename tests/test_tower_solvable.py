"""Solvability, economy and balance checks for the generated 50-floor campaign.

Run directly:

    python tests/test_tower_solvable.py

The first part proves the checked-in ``main/tower_floors.c`` still matches the
generator for floors 11-50.  Floors 1-10 are the hand-editing surface and are
deliberately excluded from the staleness comparison.  The second part replays
the whole campaign with ``tools/solve_tower.py`` and asserts that

  a) the campaign completes,
  b) no floor deadlocks (and no key is reachable only through its own door),
  c) red keys < red doors and the campaign key/door ratio stays in budget,
  d) final HP is 5-45% of the campaign HP pool (the balance target is 15-30%),
  e) every floor has at least one enemy,
  f) non-boss density is 40-50% and boss floors are sparse.

There is also a small hand-built softlock map that proves the repair pass
relocates a self-dependent key rather than merely reporting it.
"""
from __future__ import annotations

import random
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))

import gen_tower  # noqa: E402
import solve_tower  # noqa: E402


def load_generated() -> list[list[str]]:
    path = ROOT / "main" / "tower_floors.c"
    floors = gen_tower.read_generated_c(path)
    assert len(floors) == gen_tower.FLOORS, "wrong floor count"
    for index, rows in enumerate(floors):
        size = gen_tower.floor_size(index)
        assert len(rows) == size, f"floor {index + 1}: expected {size} rows"
        assert all(len(row) == size for row in rows), \
            f"floor {index + 1}: non-square row"
        flat = "".join(rows)
        assert flat.count("D") == 1, f"floor {index + 1}: down stair count"
        exit_tile = "G" if index == gen_tower.FLOORS - 1 else "U"
        assert flat.count(exit_tile) == 1, \
            f"floor {index + 1}: expected one {exit_tile}"
    return floors


def check_checked_in_file(floors: list[list[str]]) -> None:
    regenerated, _results = gen_tower.build_campaign()
    # Floors 1-10 are the hand-editing surface and intentionally no longer
    # match the scaffold; floors 11-50 are still the frozen draft.
    assert regenerated[gen_tower.REGION:] == floors[gen_tower.REGION:], (
        "main/tower_floors.c floors 11-50 are stale: "
        "run `python tools/gen_tower.py`")


def check_maps(floors: list[list[str]]) -> None:
    keys = Counter(ch for rows in floors for row in rows for ch in row
                   if ch in gen_tower.KEY)
    doors = Counter(ch for rows in floors for row in rows for ch in row
                    if ch in gen_tower.DOOR)
    gems = Counter(ch for rows in floors for row in rows for ch in row
                   if ch in "ad")
    enemies = Counter(ch for rows in floors for row in rows for ch in row
                      if ch in gen_tower.ENEMY)

    total_keys = sum(keys.values())
    total_doors = sum(doors.values())
    ratio = total_keys / total_doors
    assert 0.90 <= ratio <= 1.15, f"key/door ratio {ratio:.3f}"
    assert keys["r"] < doors["R"], \
        f"red keys {keys['r']} must be < red doors {doors['R']}"
    # Economy target: ~70 attack gems and ~55 defence gems over 50 floors.
    assert 65 <= gems["a"] <= 75, f"attack gems {gems['a']}"
    assert 50 <= gems["d"] <= 60, f"defence gems {gems['d']}"

    for index, rows in enumerate(floors):
        boss = index % gen_tower.REGION == gen_tower.REGION - 1
        walkable = sum(1 for row in rows for ch in row if ch != gen_tower.WALL)
        objects = sum(1 for row in rows for ch in row
                      if ch not in (gen_tower.WALL, gen_tower.FLOOR))
        density = objects / walkable
        if boss:
            assert density <= 0.20, \
                f"floor {index + 1}: boss floor is not sparse ({density:.1%})"
            assert not any(ch in "ad" for row in rows for ch in row), \
                f"floor {index + 1}: boss floor carries gems"
        else:
            assert 0.40 - 1e-9 <= density <= 0.50 + 1e-9, \
                f"floor {index + 1}: density {density:.1%}"
        assert sum(1 for row in rows for ch in row if ch in gen_tower.ENEMY) >= 1, \
            f"floor {index + 1}: no enemy"
    print(f"maps: {total_keys} keys / {total_doors} doors = {ratio:.3f}, "
          f"red {keys['r']}/{doors['R']}, "
          f"gems a={gems['a']} d={gems['d']}, enemies={sum(enemies.values())}")


def check_campaign(floors: list[list[str]]) -> solve_tower.CampaignResult:
    campaign = solve_tower.solve_campaign(floors)
    assert campaign.completed, \
        f"campaign failed on floor {campaign.failed_floor + 1}"

    for result in campaign.floors:
        assert result.solvable, f"floor {result.floor + 1} unsolvable"
        assert not result.self_dependent, \
            f"floor {result.floor + 1} self-dependent keys: {result.self_dependent}"
        assert not result.unreachable_keys, \
            f"floor {result.floor + 1} unreachable keys: {result.unreachable_keys}"
        assert result.enemies_total >= 1, f"floor {result.floor + 1} has no enemy"

    # Required test contract: 5-45%; the design budget is 15-30%.
    assert 0.05 <= campaign.margin <= 0.45, \
        f"final HP margin {campaign.margin:.1%} outside 5-45%"
    assert 0.15 <= campaign.margin <= 0.30, \
        f"final HP margin {campaign.margin:.1%} outside the 15-30% budget"
    return campaign


def check_repair() -> None:
    """A key behind its own door must be detected *and* relocated."""
    softlock = [
        "#######",
        "#D.yYy#",
        "#.#.###",
        "#.....#",
        "#....U#",
        "#######",
    ]
    hero = solve_tower.Hero()
    result = solve_tower.analyze_floor(softlock, 0, hero)
    assert result.solvable, "hand-built floor should be solvable after opening"
    assert result.self_dependent.get(0) == [(5, 1)], \
        f"expected the second yellow key at (5,1), got {result.self_dependent}"

    plan = gen_tower.FloorPlan(
        floor=0, boss=False, path_door_colours=["Y"], optional_doors=[],
        attack=0, defence=0, potions=0, big_potions=0, guards=0)
    repaired = gen_tower.repair_floor(
        list(softlock), 0, hero, plan, result, random.Random(7))
    assert repaired is not None, "repair returned None"
    rows, fixed = repaired
    assert fixed.solvable, "repaired floor is not solvable"
    assert not fixed.self_dependent, \
        f"repair left self-dependent keys: {fixed.self_dependent}"
    assert not fixed.unreachable_keys, fixed.unreachable_keys
    # The moved key must now be inside the region reachable without a Y door.
    start = (1, 1)
    allowed = solve_tower.reach_without_colour(rows, start, hero.keys, 0)
    yellow = [(x, y) for y, row in enumerate(rows)
              for x, ch in enumerate(row) if ch == "y"]
    assert any(cell in allowed for cell in yellow), "repaired key not reachable"
    print("repair: hand-built self-dependent yellow key relocated successfully")


def check_scaling() -> None:
    """The scaling rule must match the C contract exactly."""
    assert gen_tower.scaled_enemy("s", 0) == (24, 6, 2, 3)
    assert gen_tower.scaled_enemy("s", 49) == (141, 35, 11, 3)
    assert gen_tower.scaled_enemy("w", 25) == (260 * 350 // 100,
                                               36 * 350 // 100,
                                               22 * 350 // 100, 34)
    print("scaling: base * (100 + 10 * floor) // 100, gold unchanged")


def report(campaign: solve_tower.CampaignResult) -> None:
    print()
    print("region  floors  enemies  nonboss density  boss density  "
          "final HP  max HP  margin")
    for region in campaign.regions:
        print(f"  {region.region + 1:^4d}  {region.floors:^6d}  "
              f"{region.enemies:^7d}  {region.avg_density:^15.1%}  "
              f"{region.boss_density:^12.1%}  {region.final_hp:^8d}  "
              f"{region.max_hp:^6d}  {region.hp_fraction:^6.1%}")
    print()
    print(f"campaign: hp={campaign.hero.hp}/{campaign.max_hp} "
          f"({campaign.margin:.1%}) atk={campaign.hero.attack} "
          f"def={campaign.hero.defence} gold={campaign.hero.gold} "
          f"turns={campaign.hero.turns}")
    print(f"deadlocks: none  self-dependent keys: none  "
          f"floors with enemies: {len(campaign.floors)}/50")


def main() -> None:
    floors = load_generated()
    check_checked_in_file(floors)
    check_maps(floors)
    campaign = check_campaign(floors)
    check_repair()
    check_scaling()
    report(campaign)
    print("PASS: 50-floor campaign is solvable, deadlock-free and in budget")


if __name__ == "__main__":
    main()
