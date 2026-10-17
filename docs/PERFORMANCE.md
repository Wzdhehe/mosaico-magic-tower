# Performance pass — 15 FPS baseline

This is a **low-risk first pass** for an event-driven puzzle game, not a
hardware-confirmed temperature improvement.

## What changes

- Target frame/update cadence is reduced from 30 to **15 Hz** in the shared
  `GAME_TICK_HZ`. Device (`main/main.c`) and host
  (`main/game_module.c`) already read this constant.
- Timed notifications (3 seconds) and item-gain feedback (4 seconds) now
  derive their durations from `GAME_TICK_HZ`; holding a direction repeats
  after four ticks (~267 ms versus the previous ~233 ms).
- The CJK bitmap table has 258 glyphs and is generated in Unicode order.
  Each lookup now uses binary search instead of scanning all entries.
- Removes a redundant full-screen `ClearBackground` call in `game_render`.

No map, game rules, save format, sound generation, display driver or
framebuffer allocation changes are made in this PR. The existing sprite
animations still run, but are rendered at fewer frames per second.

## Why not skip all idle frames yet?

The project uses a Raylib Lite/GSP frame acquisition and presentation contract.
Simply skipping `BeginDrawing` or `EndDrawing` from a render callback is
**not** an established safe, event-driven presentation path. The engine also
requires a successfully presented first frame for its recovery safety
contract. A future change should use a documented/verified engine integration
for dirty-frame presentation, not guess from this game's API.

Likewise, the 24-kHz audio mixing task remains independent from UI FPS and
may contribute to power draw. Optimizing it is a separate experiment.

## Verification

Portable host regression tests:

```sh
python tests/run_game_timing.py
python tests/run_tests.py --label low-fps
```

In the full ESP-Mosaico workspace, also run:

```sh
python tests/test_integration.py
python mosaico.py game build --project projects/magic_tower
```

On hardware, compare **the same brightness, supply, volume, firmware and
play duration** against `main`. Measure or log:

| Measurement | Baseline (30 FPS) | This PR (15 FPS) |
|---|---|---|
| UI response and repeat timing | Not measured | Not measured |
| Frame render/present time | Not measured | Not measured |
| Idle/current consumption | Not measured | Not measured |
| Surface temperature after 20 min | Not measured | Not measured |
| BGM dropout / TE artifacts | Not measured | Not measured |

Use the Raylib Lite runtime render statistics if available. A reduction from
30 to 15 requested FPS cuts the maximum scheduled redraw rate by half, **not
necessarily CPU use or device temperature by half**. Thermal effect must be
measured with the actual device.

## References

- [ESP-Mosaico user guide](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/index.html)
- [Official Mosaico game development](https://github.com/esp-mosaico/esp-mosaico-vibe/blob/main/docs/game-development_CN.md)
- [Raylib Lite scheduler](https://github.com/espressif2022/raylib-lite-engine/blob/main/src/runner/raylib_lite_runner.c)
