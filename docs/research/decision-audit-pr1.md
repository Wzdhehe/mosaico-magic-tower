# PR #1 × PR #2: decision-focused level audit

This audit is based on [PR #1 (handcrafted floors)](https://github.com/Wzdhehe/mosaico-magic-tower/pull/1)
and [PR #2 (research and probes)](https://github.com/Wzdhehe/mosaico-magic-tower/pull/2).
It is deliberately submitted **on top of PR #1**. It does not edit gameplay,
save format, artwork, or renderer.

## 1. What changed after the PR #1 floor redesign?

| Measure | Old maps studied by PR #2 | PR #1 maps |
|---|---:|---:|
| Wall cells / all map cells | 74.4% | 63.6% |
| Unique wall layouts among floors 3–10 | Repeated (5/8/10 and 7/9 identical) | 8 of 8 |
| Objects among non-wall tiles | 39.0% | 27.5% |
| Monsters across the 10 floors | 20 | 20 |
| Floor 4 enemies | 0 | 0 |
| Red keys vs red doors | 4 : 4 | 4 : 4 |
| One **feasible** full-playthrough route | 612 actions, 568 turns, 75 HP, 479 gold | 499 actions, 466 turns, 375 HP, 379 gold |

The new maps have spatial diversity and important lock chains, but the added
empty space dilutes interactions. Conversely, neither a quiet floor nor a
4:4 red-key/door ratio is automatically a defect.

**Do not optimize for arbitrary quotas.** PR #2's 8–20 enemies per floor,
42–52% walls, red keys strictly fewer than doors, 2–4% remaining HP, and
12–20% fighting turns are design references from other towers, not universal
hard constraints for this device, difficulty level, or campaign length.
Ten crowded 11×11 floors could be worse than ten considered floors.

## 2. The 9F fork: two routes, but what does the player actually gain?

A representative PR #1 playthrough enters 9F with
HP 701 / ATK 62 / DEF 28 / GOLD 169. The tool reads **enemy stats and maps
from C source**, then finds one lowest-static-damage route for each alternative.
It replays pickups *in the order encountered*.

| Path to exit | Steps | Fixed-stat combat loss | Simple path replay at 9F exit |
|---|---:|---:|---|
| Through golem, wraith blocked | 14 | 6 HP | HP 695, ATK 67, DEF 28, GOLD 195 |
| Through wraith, golem blocked | 20 | 48 HP | HP 751, ATK 67, DEF 31, GOLD 203 |

Why the apparent reversal? The wraith route **first collects a defence gem**;
the wraith then costs 30 HP, not 48; a large potion adds 80 HP afterwards.
This is a classic Magic Tower sequence question.

However, removing either the golem *or* the wraith still leaves **the same
six item positions** geometrically accessible, once the other path is taken.
Therefore the current fork is largely a *timing/route-length distinction*,
not two exclusive reward packages. That can still be useful, but it needs
actual strategic tradeoffs rather than a purely decorative fork.

**Important limitation:** these are two candidate paths, NOT all possible
full-floor strategies. The path search freezes ATK/DEF and treats doors as
blocked; the replay then applies simple-path pickups in real order. It does
not explore multi-floor decisions, shopping, returning later, intentional
extra battles, or globally optimized outcomes. The stats are examples,
not claims of a globally optimal win route.

### Proposed 9F redesign criteria

- Either guardian should lead to a different *early* resource opportunity
  (e.g. attack breakpoint vs defence/HP); make the reward visible and legible.
- Both choices must be viable at several plausible hero builds, not just
  ATK 62 / DEF 28; record HP and gold after both outcomes.
- If both rewards can eventually be collected, timing must matter before
  10F: a route should plausibly reduce a future enemy's damage by a round.
- Add explicit test scenarios for low-attack, balanced, and high-defence
  entries; only then adjust monster numbers or map items.
- Do not add one-way traps, random fights, or untelegraphed permanent lockouts.

## 3. Verified geometry worth preserving

- 5F red seal cannot be bypassed, so the 4F blue-key / 3F red-vault
  backtrack is relevant.
- 7F wraith geometrically gates the red key used on 8F.
- 8F red seal gates the climb.
- 9F requires one of two guards; removing both blocks the exit.
- 10F boss is unavoidable.

These are verified as **wall/gate topology properties** only. The new
\`tools/audit_tower_decisions.py --check\` and portable unit tests protect
these specific intentions instead of requiring every floor to contain
eight monsters.

## 4. PR #2 gameplay problems still open

1. **Gold has weak strategic use.** The sample PR #1 winning route still
   finishes with 379 gold; optional purchasing and breakpoints deserve
   analysis before shop price changes.
2. **Sentinel save inconsistency.** The C battle preview's default sentinel
   rewards 0 gold, while \`enemy_gold(SENTINEL)\` returns 60, and state
   validation additionally forbids consuming it. PR #2 reproduces the
   problem. Fix separately with a gameplay/save regression test.
3. **No recoverable failure mode.** Refusing unwinnable fights avoids
   accidental deaths but can cause irreversible soft locks after bad
   allocations. Prefer a transparent restart/retry facility to punishing
   blind HP reductions.
4. **Narrative is currently mostly hints.** The next content pass could
   make one visible clue change the player's preferred route; a new puzzle
   should not require a heavy event engine.
5. **Replayability needs several *viable* builds**, not randomness. Compare
   an attack-first, defence-first and key-saving plan; avoid arbitrary hidden
   doors or unbounded grinding.

## 5. Proposed content sequence (future, separate PRs)

1. **9F and boss economy:** create genuinely different attack/defence
   breakpoints and consumable timing; record ≥2 successful whole-tower
   replays with different decisions. Rebalance without requiring players
   to memorize the correct route.
2. **Softlock and sentinel reliability:** protect saves and give explicit
   recovery options. No surprise difficulty spikes.
3. **3F–8F breadcrumbs:** use short NPC lines and existing coloured doors
   for one additional optional early/late reward loop, with visible payoff.
4. **Small challenge goals:** fewer unnecessary kills / more remaining HP /
   fewer doors opened. Track only data already available; no new animations.

## 6. Reproduction

Run against PR #1's branch:

\`\`\`sh
python tools/audit_tower_decisions.py
python tools/audit_tower_decisions.py --check
python -m unittest discover -s tests -p test_tower_decisions.py
python tests/run_tests.py --label design-audit
\`\`\`

The static checker never claims softlock freedom, balanced alternate routes,
or hardware performance. Those need a stateful multi-floor solver and
playtesting. It only fails CI for level-defining progression invariants.

## Sources

- [Open-source 魔塔 engine (mota-js)](https://github.com/ckcz123/mota-js)
- [24F reference tower](https://github.com/arcxingye/MagicTower)
- [H5魔塔 authors' design pitfalls](https://h5mota.com/bbs/thread/?p=1&tid=1183)
- [H5魔塔 level-layout examples](https://h5mota.com/bbs/thread/?p=6&tid=1152)
- PR #2's research documents and model probes
