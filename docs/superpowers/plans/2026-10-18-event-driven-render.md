# Operation-driven Render Implementation Plan

> Use subagent-driven-development for the portable state scheduling and independent review; the parent integrates renderer and official adapter sequentially. User authorized implementation in this chat.

**Goal:** Stop idle rasterization and display submissions while retaining correct interaction and first-frame acceptance.

**Architecture:** Instance-owned pending-frame state, official engine idle callback, static shared C drawing, explicit forced Host screenshots and pending-only device rendering.

**Tech Stack:** C11, existing pinned Raylib Lite Engine/GSP, 15 Hz maximum active update cadence, official CLI build.

- [x] Inspect runtime frame acquisition/presentation, idle callback and uncommitted engine changes. Prepare an isolated checkout from PR #3 and run its portable baseline.
- [x] Add meaningful failing tests, then implement `game_needs_redraw`, `game_render_complete`, `game_is_idle` and sticky operation feedback in game.c/h. Preserve keyboard suppression, audio and UI behavior.
- [x] Freeze tick-based sprites in game_view.c. Add pending-only render path, clear dirty only after frame acquisition/presentation succeeds, and wire main.c's official idle callback. Device dropped-frame statistics restore pending after a rejected submission.
- [x] Add real-render scheduling regression with acquisition/present counters, frame failure/retry, static pixels and fresh-target screenshots. Run existing timing, model and interaction tests. Reverting static sprites or rejection recovery makes the new tests fail.
- [x] Validate unchanged first-frame/Iris/recovery paths and compile via official workspace without replacing current user projects or upstream changes. Official build exit 0; 1,410,464-byte firmware, 3 reported warnings.
- [x] Independent specification/quality review, address blockers, retain test evidence and update docs/HANDOFF.
- [ ] Commit and update PR #3 after validation; leave merge and hardware installation untouched. User also requested the separate map optimization; integrate and verify both afterward.
