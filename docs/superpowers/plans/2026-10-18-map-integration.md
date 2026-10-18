# Map Integration Implementation Plan

> For agentic workers: use subagent-driven-development for portable map tests and independent review. User authorized the existing PR #1/#6 map design and requested execution in this chat.

**Goal:** Deliver the optimized ten-floor campaign together with operation-driven rendering.

**Architecture:** Keep map/economy data and model in tower.c; preserve existing geometry and guarded-fork design. Restore structural checks, exercise real C strategy routes and regenerate source-derived CJK glyphs. Validate both independent PRs in one local combined checkout.

**Tech Stack:** C11, Python test drivers, official pinned Raylib Lite RGB565 Host and ESP-IDF toolchain.

- [x] Inspect PR #1/#6 and run existing portable model/guarded-fork baselines. Audit found missing structural test and 30 missing Chinese glyphs; four real C strategy audits independently pass.
- [x] Worker owns tests/test_level_design.py, new real-C strategy test/runner, and guarded-fork CI. Restore geometry/key/boss gates; persist all four routes with per-action save/import and expected terminal states. Run and retain evidence.
- [x] Parent adds failing glyph-coverage regression, derives generator input from game_view.c strings, regenerates ui_font.h, reruns coverage, and records font source/resources.
- [ ] Parent updates map docs and invokes independent review. Run topology, four C routes, golden regeneration, model tests; commit/push fixes to existing PR #6 without merging.
- [ ] Parent combines performance and map commits in a new local checkout, adapts timing/shop UI expectations only where semantics changed, adds actual Host shop purchase/price checks, and regenerates native screenshots.
- [ ] Re-run combined C/model/render/Host tests and official firmware build in the dedicated validation project; update HANDOFF with exact commits, logs and artifacts. Leave PRs draft and hardware untouched.
