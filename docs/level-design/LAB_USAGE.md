# C design replay lab

`tools/tower_design_lab.py` compiles a snapshot of the current `main/tower.c`,
`tower.h`, and `tower_floors.h` with supplied designer rows. It never edits
production sources. Combat, item effects, stairs, teleport rules, and save
validation all run in C.

```python
from tools.tower_design_lab import build, Replay

lib = build(floors, "artifacts/my-design-check")
with Replay(lib, floors, "artifacts/my-design-check", "route-name") as replay:
    replay.go(7, 9)
    replay.preview(12)  # TOWER_TILE_SLIME, evaluated by tower_enemy_preview
    replay.stair(True)
    replay.checkpoint("entered second floor")
```

Each floor supplies `.rows`, or is a dictionary containing `rows`. Dimensions
come from the actual rows. Supply up to `TOWER_FLOORS` floors in table order;
only the missing tail receives explicit 5x3 inert fixtures. These fixtures are
not campaign content. The output includes model snapshots, provenance hashes,
compiler diagnostics, and a JSON action trace written by `close()`.

- `state()` returns the C snapshot, including pending combat details. Its floor
  index is zero based.
- `go(x, y)` uses cardinal BFS within the current descriptor dimensions. Loot
  may be traversed. Doors, enemies, stairs, goals, secrets, and events must be
  the exact target before the helper interacts with them. Consumed secrets and
  events can subsequently be traversed when C reports ordinary floor.
- `step(dx, dy, allow_death=False)` moves, saves the combat preview if produced,
  then confirms a winnable fight. Explicitly allowing death permits lethal
  confirmation. A failed action or blocked route produces a diagnostic.
- `stair(up)` explicitly enters one stair; `floor(number)` uses one-based floor
  numbering and follows stairs until the requested floor is reached.
- `buy(kind)` requires the player to stand on a C shop tile; `kind` is the C
  purchase type, `0` attack or `1` defence.
- `preview(tile)` returns C enemy fields: tile, HP, attack, defence, gold,
  damage, rounds, and winnable. It does not initiate combat.
- `warp(anchor)` passes the C zero-based anchor index, `0..5`, directly to
  `tower_teleport`. A locked warp is recorded with C's refusal status.
- `checkpoint(label=None)` stores the current snapshot. `moves` and
  `checkpoints` remain available for custom report assertions.

Every action, including refused actions, standalone preview calls, combat
previews, confirmations, and warps, validates export/import/export byte equality.
Route searches have a finite bound based on the actual floor area and stop if
an attempted step neither changes position nor consumes the targeted tile.

For a designer-owned mechanical experiment, `build` accepts
`model_transform(c_source) -> str` and `headers={"extra.h": "..."}`. The transform
applies only to the copied C source. Extra headers cannot replace the copied
model files; provenance records original and compiled source hashes. Use a
fresh output directory for a new build while an earlier Windows DLL is loaded.
