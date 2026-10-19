# Roadmap

## Shipped

- Ten hand-authored 11×11 floors with unique plans, eight stair arrangements,
  distinct scene palettes and the floor 3 → 4 → 5 key detour.
- A key-between-guards fork on floor 6, a wraith-blocked exit on floor 9,
  the floor 7 → 8 red-key chain, the floor 5 cistern seal, and a shared
  rising merchant price.
- Replayable route fixtures: one golden route plus four guard combinations
  replayed in the real C model with a save round-trip after every action.
- Unified settings (language, volume, brightness, repository address) with
  restart-safe preferences, and three original 16-bar soundtrack loops.
- Operation-driven rendering and a serialized display-brightness transaction;
  tested against the real engine source and validated on hardware.

## Next: larger map

The campaign fits one 11×11 screen per floor. The next iteration keeps the
authored floor content but renders a larger floor through a camera that follows
the player, so rooms can span more than one screen:

- a camera/viewport layer in `game_view.c` instead of a fixed 11×11 grid
- floors authored at a larger size without changing the tile vocabulary
- the same portable model tests, extended to the larger geometry
- regenerate `tests/win_route.json`/`win_route.h` after every map change

## Later

- more boss phases and scene-specific hazards on the summit
- a second merchant in the upper tower so late gold has a use
- optional minimap of visited floors
- more shop choices and consumable inventory
- additional chiptune tracks for the boss and ending
