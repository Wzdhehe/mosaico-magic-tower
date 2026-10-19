> **Status (2026-10-18):** the brightness hotfix is installed and confirmed on the physical device. See [CHANGELOG](../CHANGELOG.md) for the release summary.

# Unified settings and original music, 2026-10-18

This feature builds on `b4ce118` (the combined map and demand-rendering branch).
Release branch: `feat/settings-music-v1.2`. Device validation used the official
`projects/magic_tower_demand_render` application inside the ESP-Mosaico workspace.

## Behavior and ownership

One gear replaces the old language and speaker buttons. Its opening touch
region matches the original language button; the visual gear remains small.
The panel captures input, including drags and held keys, and contains Chinese /
English, volume 0–100, brightness 10–100, and the readable two-line repository
address `github.com/Wzdhehe/mosaico-magic-tower`. The title credits Ponkan.
There is no fake browser launch or QR dependency.

Preferences default to Chinese / volume 45 / brightness 40. Changed preferences
are saved once on close, with a visible error and retry after failure. Device
storage is the separate `magic_tower/prefs1` NVS u32. Host storage is the separate
four-byte `magic_tower.save.prefs`, replaced transactionally. Neither new game
nor journey load changes preferences. The journey remains v4 and 2048 bytes.
No NVS erase, partition change, radio change or QSPI frequency change is used.

Brightness calls `bsp_display_brightness_set()` on the game task, coalesced to
game updates or a final close flush. Volume publishes an atomic control; only
the audio task owns the synthesizer. Static panels settle without redraws.
Held RESET/PAUSE/START keys survive pointer capture until their real release;
the review-discovered leak has a failing-before/passing-after regression.

The three original 16-bar themes use four voices and one fixed echo buffer. The
codec opens at 100% output and a 2.25x makeup gain keeps full-volume peaks
around 13k/32768, roughly the comfortable level previously heard at 45%. The
mixer still clamps as a safety net.
The portable C core also produces the WAV previews; Python only writes headers
and invokes that core. No sample files or external runtime synthesizer are
needed. See `assets_src/music/README.md` for score provenance, keys, tempos,
memory ownership, signal metrics, test commands and limitations.

## Reproduced checks

All passed in this session:

- Portable model, level design, guarded forks, font coverage (256 sorted glyphs).
- Four complete strategy routes, with a 2048-byte save round-trip after every
  action (485 / 484 / 480 / 479 actions); generated win route unchanged.
- Timing/settings/persistence/brightness/held-action regression suite.
- Real RGB565 renderer: 300 settled polls cause zero acquisitions/presents;
  acquisition/submission failures retain the pending frame.
- Preference codec and atomic storage, including truncated/invalid records and
  a blocked staging path retaining the preceding record.
- Music allocation failure, independent instances, arbitrary chunk equivalence,
  deterministic output, exact mute, advancing muted timeline and transitions.
- Official native Host integration: **15/15**, including actual settings input,
  restart/new/load persistence, failed write/retry and malformed preferences.
- Actual C-rendered title/settings in both languages and the full campaign/shop
  screenshot driver, including all ten floors and ten NPC dialogues.
- Official headless simulation, 120 frames, exit 0, 480×480.

WAV previews are in `artifacts/music/`; screenshots in `docs/images/` and the
official project's `artifacts/`. Host tests preserve and restore journey and
preference files. They do not measure board temperature, power, physical touch,
speaker response or panel luminance.

## Build and recovery

The IDF doctor confirms target ESP32-S31, constraint >=6.2 and exact checkout
`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`. The pinned Raylib engine remains
`ca3200a6b7f63e2bdea07851e19fdbd3a42eb742`, with the pre-existing local TE changes
preserved. The Windows GSP depfile workaround and all retained recovery
contracts are preserved. Only the official `mosaico.py` build/update path is used.

The first sandboxed build stopped before compilation. Retrying the official
command in the normal environment with workspace TEMP/TMP succeeded. Preserve
both logs; do not misclassify the interrupted build as a source compilation error.
The first complete build passed in 231.427 s and produced 1,416,176 bytes;
the touch-region follow-up is rebuilt before installation.

The old installed combination is retained under
`artifacts/rollback-installed-b4ce118/`: `.bin`, `.elf`, generated sdkconfig and
partition-table binary. Old binary SHA256:
`5ac7cb5b1861c527047f536a934af7c7e487ebead568c07349ce684293f4dbc4`.
The partition-table SHA256 is unchanged:
`7f638212e0abfc152ff45cce4a3d9ff3c1312f96465a972b5be3b7eb79c8367c`.

Before the device update, the test unit was a physical ESP-Mosaico in normal
mode with the old intended firmware SHA `6f361d81160a114aec9064d002fdd0e2e70d349c1d0c3de4cf166ea62ee22bb1`.
Its screenshot showed the title with no saved journey. Crash inspection found
no valid retained core dump and no crash loop. The Iris logs and screenshots
were retained locally during validation but are not part of this repository.

The user reviewed the simulator and later confirmed the brightness interaction
on the physical device. Device identifiers and internal delivery logs are
intentionally not published.
