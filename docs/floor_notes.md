# Region I hand-edit notes - floors 1-10

Source: the **current frozen draft** in `main/tower_floors.c`
(generator seed `20261018`, SHA-256
`3075938CEE080AB2C2E2B3082E568F408A0AE35222043062957430E8577267CF`).
This is a design scaffold, not final content: floors 1-10 are intended to be
hand-edited from here.

Regenerate any sheet after an edit:

```powershell
python tools/floor_report.py --floor 4
```

The tool reads `main/tower_floors.c` directly, plus the campaign solver in
`tools/solve_tower.py`, so it always describes the current file. Key flow is
the solver's full-clear run (`entry -> gained -> spent -> carried`); "mandatory"
doors are the doors whose removal disconnects the down stair from the exit with
all other doors treated as open. "Articulation" means a cut vertex of the
non-wall graph; "gates the exit" means removing that enemy disconnects D from
U/G. "Loot behind" is the pickup set that becomes unreachable when that enemy
is removed.

## Region-I beat used by this draft

| Floor | Role | Why this role in the draft |
|---:|---|---|
| 1 | teaching | first floor; one tutorial slime, first keys/doors/potions |
| 2 | production | key-rich: 2 yellow + 1 blue + 1 red gained; spare blue key exits |
| 3 | hub | three doors / branchy layout; one forced knight |
| 4 | production | 4 keys and 6 potions; one forced knight |
| 5 | pressure | two forced knights; highest region-I enemy damage |
| 6 | shop | merchant tile (`S`) |
| 7 | pressure | forced knight guards the exit |
| 8 | hub | two forced knights and two blue doors |
| 9 | supply | five doors, four keys, no NPC; pays back the region |
| 10 | boss | sparse demon floor; no gems, no keys/doors |

## Quick watchlist

- **One-guard floors 1, 2, 4, 6, 7, 9.** A single articulation enemy gates the
  exit and nearly every pickup. These are the strongest candidates for adding
  more chokepoint guards during hand-editing (the "walking simulator" risk).
- **10F boss is avoidable.** The demon `M(7,4)`, the second knight `k(4,5)` and
  the bat `v(1,8)` are not articulation cells. Only `k(5,3)` gates the exit.
  If the boss should be mandatory, its cell has to move onto the critical path.
- **Secret is identical on all ten floors:** `%` at `(1,0)`, opening only a
  1-cell dead end and rewarding nothing. This is a hand-edit candidate.
- **No dead-end corridor is longer than 3 cells** on any region-I floor (the
  longest is the 2-cell secret stub `(1,0) -> (1,1)`).
- **Red keys trail red doors as intended.** Floors 8 and 9 each carry an
  unkeyed red door; floors 1, 2, 4 and 6 carry both a red key and a red door.
  Blue keys carry forward from 2F onward; by 9F the player enters with two.
- **2F leaves a blue key unused** (no blue door in the draft), and 8F also
  carries blue surplus. Hand-editing can either add a blue vault or cut a key.

## Floor sheets

### Floor 1 - Region I - teaching (draft)

- **Key flow:** entry y0 b0 r0 -> gained y2 b1 r1 (placed y2 b1 r1) -> spent y2 b1 r1 (mandatory Y2 B0 R0) -> carried y0 b0 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - s(3,5) - gates the exit; loot behind: a(4,8), a(7,8), r(8,1)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 2 - Region I - production (draft)

- **Key flow:** entry y0 b0 r0 -> gained y2 b1 r1 (placed y2 b1 r1) -> spent y2 b0 r1 (mandatory Y1 B0 R0) -> carried y0 b1 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - v(6,5) - gates the exit; loot behind: P(5,2), p(7,4), p(8,5)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 3 - Region I - hub (draft)

- **Key flow:** entry y0 b1 r0 -> gained y1 b2 r0 (placed y1 b2 r0) -> spent y1 b2 r0 (mandatory Y1 B1 R0) -> carried y0 b1 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(5,6) - gates the exit; loot behind: a(2,8), d(4,8), p(7,7), p(8,5), p(8,6)
- **Avoidable enemies (deletion candidates):** s(7,2).
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 4 - Region I - production (draft)

- **Key flow:** entry y0 b1 r0 -> gained y2 b1 r1 (placed y2 b1 r1) -> spent y2 b1 r1 (mandatory Y1 B1 R0) -> carried y0 b1 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(8,3) - gates the exit; loot behind: p(2,5), p(3,5), a(5,7), p(6,5), p(8,5), d(8,8)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 5 - Region I - pressure (draft)

- **Key flow:** entry y0 b1 r0 -> gained y1 b2 r0 (placed y1 b2 r0) -> spent y1 b2 r0 (mandatory Y1 B1 R0) -> carried y0 b1 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(8,3) - gates the exit; loot behind: a(1,7), p(2,4), b(4,4), P(4,8), a(7,5), d(7,7), d(8,4), b(8,7)
  - k(6,8) - gates the exit; loot behind: a(1,7), p(2,4), b(4,4), P(4,8)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 6 - Region I - shop (draft)

- **Key flow:** entry y0 b1 r0 -> gained y2 b1 r1 (placed y2 b1 r1) -> spent y2 b1 r1 (mandatory Y1 B1 R0) -> carried y0 b1 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(6,5) - gates the exit; loot behind: P(7,8), b(8,5), r(8,6)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 7 - Region I - pressure (draft)

- **Key flow:** entry y0 b1 r0 -> gained y1 b2 r0 (placed y1 b2 r0) -> spent y1 b2 r0 (mandatory Y1 B1 R0) -> carried y0 b1 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(3,5) - gates the exit; loot behind: d(4,4), b(4,7), a(5,5), p(5,6), p(5,7), P(8,2), p(8,3)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 8 - Region I - hub (draft)

- **Key flow:** entry y0 b1 r0 -> gained y1 b2 r0 (placed y1 b2 r0) -> spent y1 b1 r0 (mandatory Y1 B1 R0) -> carried y0 b2 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(3,2) - gates the exit; loot behind: p(4,4), b(7,1), a(7,2), p(7,4), p(7,8), d(8,1), p(8,3)
  - k(6,2) - gates the exit; loot behind: b(7,1), a(7,2), p(7,4), p(7,8), d(8,1), p(8,3)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 9 - Region I - supply (draft)

- **Key flow:** entry y0 b2 r0 -> gained y2 b2 r0 (placed y2 b2 r0) -> spent y2 b2 r0 (mandatory Y1 B1 R0) -> carried y0 b2 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(5,6) - gates the exit; loot behind: p(1,7), d(4,8), b(5,7), b(6,8), p(7,5), a(7,8), p(8,4)
- **Avoidable enemies (deletion candidates):** none.
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).

### Floor 10 - Region I - boss (draft)

- **Key flow:** entry y0 b2 r0 -> gained y0 b0 r0 (placed y0 b0 r0) -> spent y0 b0 r0 (mandatory Y0 B0 R0) -> carried y0 b2 r0 [solver OK].
- **Articulation enemies (all routes cross):**
  - k(5,3) - gates the exit; no pickups behind
- **Avoidable enemies (deletion candidates):** M(7,4), k(4,5), v(1,8).
- **Secret:** `%` at (1,0) -> none (only the dead-end cell opens).
- **Dead-end corridors >3 cells:** none (longest dead end: 2 cells, (1,0) -> (1,1)).
