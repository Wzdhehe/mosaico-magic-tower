# Changelog

## RC4 Iris 0.2 export — 2026-10-20

- Rebuild the application against the official Recovery ABI 2 / product v2 integration for the global Mosaico Ideas website. The `-iris02` bundle uses `esp-iris-system-update/0.2`; the original unsuffixed RC4 export remains a legacy ABI 1 file.
- Preserve game C sources, maps, music, engine revision, checkpoint format and game NVS address. This is an integration/export correction, not a base-firmware update.
- Reject obsolete or unknown SDK schemas before packaging by default; inspect the exported schema and effective product contract. Legacy export is an explicit local option.
- Requires an already matching Vibe Mode 0.2 installation. Website and hardware acceptance remain separate from offline build verification.

## v1.8.0-rc.4 — 2026-10-20 (field fixes)

- Split only the demon king's combat labels and values into separate rows below its name, so four-digit HP cannot overwrite ATK/DEF glyphs. Ordinary monster panels retain their original layout.
- Added a bilingual two-page princess conversation before the victory page, with a repeatable map interaction after continuing exploration. Outcome music persists through the conversation and its menus.
- Removed the bypassable 49F yellow door at (15,3). Existing v8 opened-door evidence remains in the key ledger; unopened and opened RC3 checkpoints both remain loadable.
- Raised the fallen melody one octave, moved the bass out of the sub-bass register, lowered its balance and restored lead gain for the small speaker. Four voices and existing memory/CPU bounds are preserved.
- Battery acquisition now uses the pinned official BSP's verified RAM profile initialization, followed by checked SOC/voltage/current/status/operation/capacity reads. Removed the FC-to-100 override. Positive current takes precedence over the delayed DSG mode flag so plug-in charging does not stay white. No application calibration or OTP writes.
- Rejected battery samples retry once immediately. If both attempts fail, retain the last validated percentage and icon for less than 15 seconds from that valid sample, then show `--`; failed polls never extend the lifetime. Expiry is independent of the next hardware poll, including scheduling jitter. Recovery adopts the actual SOC without smoothing or clamps. Failure logs include register/error or validation reason and raw telemetry. The intermittent field-read failure itself remains under investigation; the latest tolerance change has not been installed on the device.


## v1.8.0-rc.3 — 2026-10-19 (battery HUD and outcome music)

- Volume zero now fades/drains and closes playback, then blocks the audio task until a changed control. Resume primes silence and fades in; current-track time catches up without PCM output. Hardware failures retry with a bounded delay. The BSP receive channel is unchanged; actual power and speaker behavior need device acceptance.
- Persistent upper-left battery percentage, inset for rounded screen corners. Two icon appearances: white on battery power; green while charging, including full at 100% (no lightning symbol). Unknown readings show `--`.
- Checked read-only BQ27220 telemetry every five seconds on the owning UI task. Unchanged readings do not redraw, advance turns or write checkpoints. No gauge profile or charger settings are changed.
- Dedicated original victory fanfare and death lament. Settings and restart confirmations retain the outcome music; continuing exploration restores Home. Ten tracks retain the four-voice integer synth and 6272-byte audio context.
- Remove the Ideas submission kit and marketing cover from the tracked source tree; local copies remain ignored. README uses actual gameplay screenshots.
- Preserve RC2 concealed walls, aligned hints, quest-item icons and open viewport edges. Save v8 remains compatible.

## v1.8.0-rc.2 — 2026-10-19 (hidden-door readability)

- Concealed doors (`%`) now use the wall masonry and a dark wall-color overlay
  with a recessed edge, instead of a normal floor tile with a 20% dim. At low
  AMOLED brightness they read as a darker wall block rather than ordinary path.
- Viewport framing now stops on any side that has more map beyond it; sides at a
  real map edge keep the accent frame. The open edge replaces the previous
  low-contrast translucent-only cue.
- All hidden-door hints in Chinese and English now say dark wall / 暗墙 or 墙砖
  instead of dark floor / 地砖. The 8F tutorial, 11F/21F/33F reward hints,
  18F and 27F clues and 37F note are aligned with the new visual.
- The font subset was regenerated for the new 墙 glyph (517 glyphs).

## v1.8.0-rc.1 — 2026-10-19 (fifty-floor production integration)

- Integrated all fifty approved maps, five regional identities, eight concealed rooms, three return errands, reversible sluice and optional three-sigil room. Final floor is an 11×11 symmetric throne hall.
- Coordinate-specific bilingual NPC/event pages, 24px Chinese / 16px English dialogue text and distinct 47F terminal robot.
- Automatic edge paging with translucent hints; restart now offers retry/new/cancel and a second new-game confirmation. Cancelling from a modal restores that modal and page.
- Eight bounded integer-synth loops and five-region/boss/ending routing; v8 saves reject older geometry cleanly.
- Migrated portable timing, production rendering, save-format and CI coverage. Current acceptance commands and hardware limits are in README and docs/level-design/PRODUCTION_INTEGRATION.md.
- The earlier v1.8-dev was installed through official application update and verified healthy. The RC adds larger Chinese dialogue, the “五十层试炼” subtitle, aligned firmware version metadata and publishing assets; complete RC hardware playtesting remains open.

## v1.7 — 2026-10-18 (checkpoint and warp rules)

### Added

- **Automatic checkpoints.** A new run writes its first checkpoint, and every
  real floor change (stairs or a cross-floor warp) writes the next one. There is
  no manual save or load button; the title offers “继续旅程” and the death panel
  offers “从检查点重试”. A failed write is reported as `保存失败` and never
  pretends a checkpoint moved.
- **Six fixed warp anchors: 1 / 10 / 20 / 30 / 40 / 50.** The panel is a fixed
  2×3 grid, shows locked anchors explicitly, and has no paging. Warping is free
  and lands on the anchor floor's down stair.
- **Chapter unlocks.** Anchors 10/20/30/40/50 open only after the matching
  ten-floor chapter is completed: stepping on that chapter's goal, or leaving it
  through its exit stair. Merely visiting a floor does not unlock anything.
- Save format **v7** (2632 bytes) adds an explicit chapter mask. v6 and older
  records are rejected cleanly.
- Clearing the tower returns to floor 50 (“继续探索”) instead of the title, so
  the last anchor is a real teleport target and post-game exploration survives.
- `tests/run_game_timing.py` now drives the checkpoint/warp/retry rules through
  the public game API; `tests/test_tower50.c` covers the model side, including
  forged chapter masks.

### Changed

- The goal tile is a chapter gate everywhere except the last floor: floor 10's
  `G` ends chapter one and leaves the tower open; only floor 50's `G`/Demon King
  ends the campaign.
- 10F gained its missing up stair (one tile) so floors 11–50 are reachable and
  chapter two can be unlocked.

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
