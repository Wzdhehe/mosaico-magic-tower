# Operation-driven display refresh

User approved starting the performance PR work and explicitly requested static NPCs and refreshing only for user operations. This extends PR #3 (`cec9a06`) without merging the independent map/research PRs or flashing hardware.

## Behavior

- Draw the first frame unconditionally for official first-frame health acceptance.
- Keep NPCs, enemies, princess and beacon static; a settled scene must produce identical pixels regardless of idle tick count.
- A move, battle preview/confirm/cancel, story/shop transition, save/load feedback, restart, language, pause or changed volume makes the next frame pending. Repeated unchanged pointer samples do not.
- Pickup/save messages persist until superseded by another operation. No time-driven expiry creates background refreshes.
- Held keyboard movement remains responsive (15 Hz update cap / existing repeat interval), and retains the combat release suppression.
- Use the engine's existing `idle` callback to skip update/render while settled, without altering GSP/TE/USB/Iris/partition contracts. Rendering failure keeps the frame pending for retry.
- Explicit Host screenshot rendering always fills the caller's target, even when the device would be idle. A separate pending-only path exercises device render scheduling and counts zero idle acquisitions/presents.
- Audio remains independent. A change of music track after start/floor transition still reaches the audio callback.

## Scope and acceptance

Application code only; preserve existing local uncommitted engine TE changes. No radio/clock/display bus changes, no hardware writes, no unsupported temperature/current claims. Tests must prove first-frame retry, zero idle draw/present calls, all relevant operations invalidating, no invalidation from repeated pointer samples, sticky feedback, held input and full C replay. Inspect real native C frames and build with the pinned official workspace. Keep source, tests, resources and handoff inside this project.
