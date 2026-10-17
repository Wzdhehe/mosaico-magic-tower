#!/usr/bin/env python3
"""Audit how shop purchases change the final boss's deterministic damage.

Source of truth: the current C monster/merchant rules and the checked-in
successful route. No map rewrite, full-route optimization or device claim.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def constant(header: str, name: str) -> int:
    hit = re.search(rf"^\s*#define\s+{re.escape(name)}\s+(\d+)u?\s*$",
                    header, re.MULTILINE)
    if hit is None:
        raise ValueError(f"Missing or nonliteral constant {name}; review the model")
    return int(hit.group(1))


def enemy_stats(source: str, name: str = "DEMON") -> dict[str, int]:
    # Require all four fields from the same C switch case, not magic numbers.
    regex = (rf"case TOWER_TILE_{re.escape(name)}:\s*"
             r"p->hp\s*=\s*(\d+);\s*"
             r"p->attack\s*=\s*(\d+);\s*"
             r"p->defence\s*=\s*(\d+);\s*"
             r"p->gold\s*=\s*(\d+);\s*break;")
    result = re.search(regex, source)
    if result is None:
        raise ValueError(f"Cannot parse TOWER_TILE_{name}; review this audit")
    return dict(zip(("hp", "attack", "defence", "gold"), map(int, result.groups())))


def battle_damage(enemy: dict[str, int], hero_atk: int, hero_def: int) -> int | None:
    """Same attack/rounds/retaliation rule as tower_enemy_preview()."""
    if hero_atk <= enemy["defence"]:
        return None
    rounds = (enemy["hp"] + hero_atk - enemy["defence"] - 1) // (
        hero_atk - enemy["defence"])
    return (rounds - 1) * max(0, enemy["attack"] - hero_def)


def reference_preboss(route: dict, enemy: dict[str, int]) -> dict[str, int]:
    """Reconstruct reference *just-before-boss* quantities from final state.

    Checked-in replay must end with a final boss, followed only by empty
    cells and the goal. If level scripting changes, this needs fresh audit.
    """
    final = route["expected"]
    fights = route["enemy_encounters"]
    if not fights or fights[-1]["name"] != "Demon King" or not final["cleared"]:
        raise ValueError("Reference route no longer ends with a Demon King win")
    return {
        "hp": final["hp"] + fights[-1]["damage"],
        "attack": final["attack"],
        "defence": final["defence"],
        "gold": final["gold"] - enemy["gold"],
        "recorded_boss_damage": fights[-1]["damage"],
    }


def allocation(enemy: dict, reference: dict, cost: int, gain: int,
               buy_attack: int, buy_defence: int) -> dict:
    spent = cost * (buy_attack + buy_defence)
    if spent > reference["gold"] or buy_attack < 0 or buy_defence < 0:
        raise ValueError("Cannot afford that upgrade allocation")
    atk = reference["attack"] + gain * buy_attack
    defence = reference["defence"] + gain * buy_defence
    damage = battle_damage(enemy, atk, defence)
    return {
        "atk_buys": buy_attack, "def_buys": buy_defence,
        "gold_spent": spent, "atk": atk, "defence": defence,
        "boss_damage": damage,
        "hp_after_boss": reference["hp"] - damage if damage is not None else None
    }


def analyze(source: str, header: str, route: dict) -> dict:
    enemy = enemy_stats(source)
    cost, gain = constant(header, "TOWER_SHOP_COST"), constant(
        header, "TOWER_SHOP_GAIN")
    if cost <= 0 or gain <= 0:
        raise ValueError("Shop must have positive price and gain")
    ref = reference_preboss(route, enemy)
    expected_damage = battle_damage(enemy, ref["attack"], ref["defence"])
    if expected_damage != ref["recorded_boss_damage"]:
        raise ValueError("Boss combat formula and reference replay disagree")
    if ref["gold"] < 0:
        raise ValueError("Impossible reference currency")
    feasible = []
    max_buys = ref["gold"] // cost
    for total_buys in range(max_buys + 1):
        for atk_buys in range(total_buys + 1):
            feasible.append(allocation(enemy, ref, cost, gain,
                                       atk_buys, total_buys - atk_buys))
    candidates = [r for r in feasible if r["boss_damage"] is not None]
    best = min(candidates,
               key=lambda r: (r["boss_damage"], r["gold_spent"],
                              r["def_buys"]))
    zero_damage = [r for r in candidates if r["boss_damage"] == 0]
    cheapest_zero = (min(zero_damage,
                         key=lambda r: (r["gold_spent"], r["def_buys"]))
                     if zero_damage else None)
    return dict(enemy=enemy, reference=ref, shop_cost=cost, shop_gain=gain,
                baseline=allocation(enemy, ref, cost, gain, 0, 0),
                first_attack=allocation(enemy, ref, cost, gain, 1, 0),
                first_defence=allocation(enemy, ref, cost, gain, 0, 1),
                best=best, cheapest_zero=cheapest_zero,
                candidates=feasible)


def report(r: dict):
    e, ref = r["enemy"], r["reference"]
    print(f"Boss HP {e['hp']} ATK {e['attack']} DEF {e['defence']}")
    print(f"Reference BEFORE boss: HP {ref['hp']} ATK {ref['attack']} "
          f"DEF {ref['defence']} GOLD {ref['gold']}")
    print(f"Merchant: {r['shop_cost']} gold -> +{r['shop_gain']} ATK or DEF")
    print("These are mathematical upgrades assuming the player returns to"
          " the 2F shop before boss; no complete alternative route is proven.")
    print(" ATK buys | DEF buys | Gold spent | Boss damage | HP after boss")
    interesting = {(0, 0), (1, 0), (0, 1), (4, 0), (2, 2),
                   (0, 4), (7, 0), (0, 7)}
    for a, b in sorted(interesting, key=lambda pair:
                       (sum(pair), pair[1])):
        if (a + b) * r["shop_cost"] > ref["gold"]:
            continue
        item = allocation(e, ref, r["shop_cost"], r["shop_gain"], a, b)
        print(f" {a:8d} | {b:8d} | {item['gold_spent']:10d} | "
              f"{str(item['boss_damage']):11s} | "
              f"{str(item['hp_after_boss'])}")
    print("Cheapest zero-damage option:", r["cheapest_zero"])
    print("Lowest-damage affordable option:", r["best"])
    print("LIMIT: No route travel, earlier combat, optional loot or shop"
          " accessibility simulation. Currency excludes boss's future reward.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="Validate source/replay consistency (does not enforce balance)")
    args = parser.parse_args()
    src = (ROOT / "main" / "tower.c").read_text(encoding="utf-8")
    hdr = (ROOT / "main" / "tower.h").read_text(encoding="utf-8")
    route = json.loads((ROOT / "tests" / "win_route.json").read_text(encoding="utf-8"))
    result = analyze(src, hdr, route)
    report(result)
    if args.check and result["baseline"]["boss_damage"] is None:
        raise SystemExit("Reference cannot attack boss")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
