# Battery transient-read implementation plan

**Goal:** Implement the user's approved retry + bounded last-good display policy, then publish the pending RC4 source changes in a reviewable GitHub PR.

**Architecture:** Keep official BSP initialization, checked reads, validation and the existing single UI-task owner. Each scheduled poll gets at most two complete sample attempts, without a sleep or another task. Only valid samples update the last-good reading and timestamp. Failed attempts may reuse it for strictly less than 15 seconds; at 15 seconds it expires to unknown. Startup without a valid reading remains unknown. Five-second normal polling and the existing bounded one-second startup retry window remain unchanged. Diagnostic warnings retain the failing register/error or validation reason plus raw fields; recovered retries also record the valid sample.

**Tech stack:** C11 ESP-IDF application; fault-injected portable C runner; official pinned firmware build; GitHub CLI.

The user approved this design in the conversation. No new screen, gauge calibration, save format or hardware update is part of this task.

- [x] Extend `tests/run_battery.py` for one failed transaction, invalid snapshot retry, failure of both attempts, last-good expiry at exactly 15 seconds, repeated failure without timestamp renewal, recovery without percent clamps, unknown startup and bounded idle reads. Observe the new assertions fail on existing source.
- [x] Update `main/game_battery.c/h` with bounded retry, last-good timestamp/cache and failure diagnostics. Keep profile and charger ownership unchanged. Run the regression to green.
- [x] Update README, changelog, battery design and firmware availability documentation. Distinguish the source candidate from published RC2 and from the older RC4 package currently on the device.
- [x] Run the complete campaign checks; build the official application offline. Review changes before publishing.
- [ ] Publish a normal branch based on current remote main with the validated RC4 changes; avoid rewriting unrelated remote commits or adding ignored marketing/diagnostic files. Create/attach the PR, check CI, report its URL and any remaining hardware validation gap. Do not flash or merge as part of this request.
