# Roadmap

## Shipped

- Hand-authored ten-floor campaign with asymmetric stairs, supply alcoves,
  dead ends and the floor 3 → 4 → 5 key detour (PR #1).
- Guarded attack/defence forks on floors 6 and 9, the floor 7 → 8 red-key
  chain, the floor 5 archive seal, and a shared rising merchant price (PR #6).
- Replayable route fixtures: one golden route plus four guard combinations
  replayed in the real C model with a save round-trip after every action.

## Next: larger map

The campaign fits one 11×11 screen per floor. The next iteration keeps the
authored floor content but renders a larger floor through a camera that follows
the player, so rooms can span more than one screen:

- a camera/viewport layer in `game_view.c` instead of a fixed 11×11 grid
- floors authored at a larger size without changing the tile vocabulary
- the same portable model tests, extended to the larger geometry
- regenerate `tests/win_route.json`/`win_route.h` after every map change

## Later

- distinguish the last floors with unique tiles/ambience and more boss phases
- optional minimap of visited floors
- more shop choices and consumable inventory
- additional chiptune tracks for the boss and ending
- on-demand rendering on the device (tracked as a separate performance PR)
