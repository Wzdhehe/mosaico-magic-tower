# Operation-driven rendering

The square 480×480 UI draws its first frame, then redraws after gameplay or UI operations. Idle NPCs, enemies, the princess and the goal beacon use fixed sprite frames. Pickup and save/load feedback stays visible until a relevant operation replaces it; no countdown wakes the renderer just to erase text.

## Scheduling and safety

- The pinned engine's official `mosaico_game_app_config_t.idle` callback skips update/render work after the frame settles. The engine still polls input at the configured **15 Hz** cadence. This does not put the entire MCU to sleep.
- A pending-frame flag belongs to each game instance. Input that changes the screen marks it pending. Repeated touch coordinates and unchanged volume do not request extra frames. Held movement keeps its four-tick repeat cadence; entering combat/dialogue suppresses held directions until release.
- The engine presents the initial frame before its normal idle loop and keeps its existing Iris health/recovery sequence. No upstream engine change is required for this scheduling feature.
- Failure to acquire a framebuffer leaves it pending. The device adapter also compares the official dropped-frame counter around submission: a rejected frame requests a retry, including when it was the user's last operation.
- `game_render()` always draws into an explicitly requested Host screenshot target. The device uses the pending-only adapter. Music track delivery is independent of the pending-frame flag, so a screenshot cannot swallow an audio change.
- Font lookup uses binary search across 241 sorted glyphs and one full-canvas clear. The map-integration campaign and its v4 save format are included; the partition layout is unchanged.

The audio task remains 24 kHz, now using the bounded original soundtrack core.
Touch polling, Iris and display services retain their settings. There are no new
clock, radio or power-management settings. Reduced redraw work is established
by the host test; current draw and temperature require measurements on the device.

## Power and display audit (2026-10-18)

The installed baseline's generated `build/sdkconfig` has
`# CONFIG_PM_ENABLE is not set` and `# CONFIG_BT_ENABLED is not set`.
Consequently automatic light sleep is not enabled in this build; blaming a
continuously held Wi-Fi power-management lock is unsupported here. Iris TCP is
compiled in, but its `tcp_start()` initializes the TCP/IP core and explicitly
leaves interfaces, Wi-Fi and addressing to the product. The inspected normal
game/engine/recovery-support startup path does not call `esp_wifi_start()` or
start the optional peer-link service. This source audit does not measure a
companion chip's physical power rails or RF current.

GSP's active tick is 10 ms while input/animation/scrolling is active. Its
separate idle-application and idle-touch policies mean this setting alone does
not establish permanent 100-Hz rendering. The game still caps active updates
at 15 Hz and skips settled rendering. The engine's separate touch task currently
polls at 12 ms, so reduced rendering does not mean that all services sleep.

A complete 480x480 RGB565 payload takes at least 23.04 ms on a 40-MHz four-lane
QSPI link, before protocol overhead. That bandwidth calculation alone neither
measures present tearing nor proves 80 MHz is required. The current engine uses
TE composition; the user reports no visible tearing. Preserve the current
official bus configuration unless an actual display regression is reproduced.

The BSP already exposes `bsp_display_brightness_set()` through the CO5300
driver. The new settings integration uses that API rather than issuing raw
panel commands. Host tests cannot validate physical luminance or power savings.

## Verification

```sh
python tests/run_game_timing.py
python tests/run_tests.py --label demand-render
# Set MOSAICO_ENGINE_ROOT if outside the official workspace's projects/ tree.
python tests/run_render_schedule.py
python tests/test_integration.py
python mosaico.py game build --project projects/magic_tower_demand_render
```

`run_render_schedule.py` compiles the real pinned RGB565 rasterizer, game view and device retry adapter. Only frame acquisition/presentation and runtime statistics are replaced by a counting/failure-injection port. Its staged Host port header redirects the reduced Host game API include to the actual device API; upstream files are untouched. Assertions cover initial acquisition failure, submission rejection/retry, fresh screenshot targets, static pixels, sticky feedback, UI operations and repeated unchanged touch samples.

| Scenario | Host result |
|---|---|
| 300 idle polls on title | 0 new acquisitions / presentations |
| 300 idle polls after a pickup | 0 new acquisitions / presentations |
| Acquisition or submission rejected | Pending retained; next accepted frame settles |
| Repeated unchanged volume drag sample | 0 additional presentations |
| Explicit screenshot into cleared target | Complete, identical RGB565 image |

The portable state suite also replays the complete ten-floor route and covers combat, dialogue, both shop upgrades, saves, language, pause/reset edge handling, held-direction suppression and music changes after a forced render.

For hardware comparison, keep brightness, supply, volume and play duration the same on both builds. Record idle current, render/display statistics, touch response, audio continuity, tearing and surface temperature. These physical measurements have **not** been performed for this change.

## References

- [Pinned engine idle/first-frame scheduler](https://github.com/espressif2022/raylib-lite-engine/blob/ca3200a6b7f63e2bdea07851e19fdbd3a42eb742/components/mosaico_game_app/mosaico_game_app.c)
- [Pinned device frame port](https://github.com/espressif2022/raylib-lite-engine/blob/ca3200a6b7f63e2bdea07851e19fdbd3a42eb742/components/mosaico_raylib_port/mosaico_raylib_port.c)
- [Official Mosaico game development](https://github.com/esp-mosaico/esp-mosaico-vibe/blob/main/docs/game-development_CN.md)
