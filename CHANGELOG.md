# Changelog

## v1.6 — 2026-10-18

### Added

- Ten new hand-authored 11×11 floor plans with eight different up/down stair
  arrangements. Stairs now land on the matching stair tile of the destination
  floor instead of a hard-coded `(5, 9)` / `(5, 1)` cell.
- Ten scene palettes and wall/floor treatments: gatehouse, armory, archive,
  moss garden, cistern, barracks, crypt, sand tomb, star terrace and throne.
  The floor header names its scene in Chinese and English.
- A single carpeted boss aisle on the summit, so the Demon King cannot be
  walked around; later floors use tier-appropriate enemies instead of idle
  zero-damage guards.
- Chinese NPC dialogue now automatically drops from the 24 px layout to a
  16 px, 17-character layout when the copy needs more than five lines. If it
  still needs more than seven lines, the dialog truncates and the layout test
  fails rather than shrinking to an unreadable size.
- `tests/test_ui_layout.py` checks NPC names, both Chinese layout budgets, the
  English wrapped-line budget and the settings GitHub address width. CI runs
  the same check. The regenerated font subset contains 256 sorted glyphs.

### Changed

- The 5F red seal now uses the 3F red key; 7F's east crypt contains the red key
  for 8F; 4F's blue vault supplies both blue keys; 6F's yellow key sits between
  a mage and a golem; 9F's exit is guarded by a wraith.
- The generated winning route is now 678 actions / 623 turns / 39 fights and
  ends at HP 387, ATK 97, DEF 31, gold 930. The route exercises the cross-floor
  key detour and the guarded exits.
- Journey saves are now **v5**. v1.5's v4 snapshots are rejected cleanly
  because the floor geometry changed; start a new journey after updating.

### Validation

- `tests/test_level_design.py` checks scene-plan uniqueness, asymmetric stair
  pairs, wall/object density, key/door budget and every mandatory gate.
- Model, timing, four strategy routes, level design, layout, glyph coverage,
  and Host integration **15/15** pass. Floor screenshots are regenerated from
  the real C renderer.
- Not flashed in this change; v1.5 remains the installed device build until an
  explicit install is requested.

## v1.5 — 2026-10-18

### Changed

- The welcome page renders “MOSAICO ORIGINAL”, the Ponkan credit and the
  version at double size; the credit moved directly under the header.
- NPC dialogue now uses a 96×150 portrait that fills the left column instead of
  the previous 26×26 map sprite, so the character occupies a meaningful share
  of the dialog. The 24 px Chinese text area was narrowed to 11 characters per
  line and now avoids starting a line with closing punctuation.

### Validation

- Settings and NPC screens re-rendered from the shared C renderer; model and
  glyph tests pass.
- Installed through the official app-update path and validated healthy; v1.5
  is the current device build.

## v1.4 — 2026-10-18

### Fixed

- Recalibrated the music output after the v1.3 loudness increase was reported
  as too loud and distorted at 100%. The makeup gain is now **2.25×** instead
  of 5×, so the maximum volume matches the level that was comfortable at 45%
  in v1.3, with ample headroom for the speaker.

### Validation

- Music core peaks are now about 13,311 / 12,136 / 12,546 of 32768, with no
  clipping on the shipped scores.
- Installed through the official recovery-first app-update path and validated
  healthy; v1.4 is the current device build.
- Settings and gameplay behavior are otherwise unchanged from v1.3.

## v1.3 — 2026-10-18

### Changed

- The settings panel renders the GitHub address at twice the previous size.
- Chinese NPC dialogue uses a 24 px glyph renderer (1.5× the base bitmap) with
  line wrapping; the NPC name matches. This is larger than the original 16 px
  text but smaller than the intermediate 32 px build.
- The speaker codec is opened at 100% output and the music core applies a 5×
  makeup gain with clipping protection. Full volume now peaks around 27–30k of
  32768 instead of ~5.5k, addressing the “volume 100 still quiet” report.

### Validation

- Music core tests pass with the new gain; peak 29,580 / 26,970 / 27,880 and
  maximum adjacent-sample jump 5,535 at 24 kHz.
- Settings and NPC screens were re-rendered from the shared C renderer.
- Installed through the official recovery-first app-update path; identity and
  health validation passed, and the device shows UI v1.3 with the larger
  settings address and 24 px NPC dialogue. Audible loudness still needs the
  user's listening check.

## v1.2 — 2026-10-18

This release unifies the settings UI, replaces the former chiptune loop with
three original soundtrack pieces, adds a hand-authored map revision, and fixes
the display freeze reported while dragging brightness.

### Added

- **Unified settings panel** behind one gear button at the top right:
  Chinese/English, volume 0–100, brightness 10–100, and the readable
  `github.com/Wzdhehe/mosaico-magic-tower` address.
- **Persistent preferences** stored separately from the journey:
  device NVS `magic_tower/prefs1`, host `magic_tower.save.prefs`. Preferences
  survive restart, new game and journey load.
- **Three original 16-bar soundtrack loops** rendered by a portable four-voice
  C synthesizer with a bounded echo buffer: title, lower tower and upper tower.
  The same core generates the WAV previews in `assets_src/music/`.
- **Hand-authored map revision**: asymmetric stairs, supply alcoves, dead ends,
  guarded attack/defence forks on floors 6 and 9, and the floor 3 → 4 → 5 and
  floor 7 → 8 key chains.
- **Rising merchant prices**: both attributes share a price sequence of
  15, 25, 35, … gold; every purchase still grants +3.
- **Operation-driven rendering** with a 15 Hz active update cap, static idle
  sprites and retry after failed frame acquisition/submission.
- **Display-control regression suite**, including tests against the real engine
  brightness transaction with mocked hardware.
- **CI workflows** for portable tests, map/route regeneration checks, guarded
  fork replays and host render scheduling.
- **Ponkan** credit on the title screen.

### Changed

- The separate language and speaker buttons are replaced by the settings gear.
  Its touch target remains the larger top-right region; the visual gear stays
  compact.
- UI version is **v1.2** (application manifest remains `0.1.0`).
- Journey save format is **v4 / 2048 bytes**. v3 records are rejected safely
  instead of being loaded into the revised map geometry; start a new journey.
- Documentation and screenshots were refreshed for the ten-floor campaign,
  settings panel and soundtrack.

### Fixed

- **Brightness freeze**: brightness changes from the game task could collide
  with GSP TE display traffic on the same SPI device. The engine now exposes
  `mosaico_game_app_set_brightness()`, which pauses presentation, retires
  fences, calls the BSP setter and resumes. Resume failures retain a token and
  retry at the start of the loop and after event/update callbacks.
- Initial brightness is applied after the startup flush and health check, so a
  pending pause cannot survive into the first frame.
- Pointer RPC now reports a failed input enqueue as an error instead of false
  success.
- Held RESET/PAUSE/START keys survive pointer capture until their real release.
- Renderer tests retain a pending frame when acquisition/submission fails.

### Validation

- Portable model, level design, guarded forks and glyph coverage: pass.
- Four complete strategy routes in the real C model: pass, including a
  2048-byte save round-trip after every action.
- Display-control wrapper failure paths: pass (pause failure, BSP failure,
  failed resume/retry).
- Music core: deterministic tracks, mute behavior, allocation failure and
  chunk-independence: pass.
- Host integration: **15/15**, including settings capture, preference
  persistence, failed-write retry and malformed records.
- Official build succeeded and the **v1.2** image was installed through the
  official Iris update path. The user confirmed on the physical device that
  the brightness interaction is fixed.
- An automated long brightness-drag stress sequence was prepared but not run
  after user confirmation; it is not claimed as evidence.

### Known limits

- Power draw, board temperature, speaker response and panel luminance were not
  measured.
- Preference persistence across a physical power cycle and long-duration stress
  were not separately measured.
- The current campaign is one 11×11 screen per floor; a larger camera-scrolled
  map remains a future feature.

### Upgrading

Existing v3 journeys are intentionally rejected because the map geometry
changed. The game starts a new journey on floor one; do not erase or rewrite
NVS to force an old record to load. Settings use their own record and default
to Chinese, volume 45 and brightness 40 when absent or invalid.
