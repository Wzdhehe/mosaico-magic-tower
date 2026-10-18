# Map optimization integration

The user explicitly requested completing map optimization after approving the
existing PR work. Adopt the existing reviewed PR #1/#6 design rather than
inventing a new campaign: ten 11×11 floors with distinct rooms/corridors,
3F/4F/5F and 7F/8F key detours, genuinely guarded attack/defence forks on 6F
and 9F, and shared progressive merchant prices (15, 25, 35…; +3 per purchase).

Finish the existing content for actual use: restore the deleted whole-campaign
topology/lock checks, persist four branch-combination replays against the real C
model (including save/load), and fix missing Chinese NPC glyphs. Generate the
font subset from the actual authored UI strings so future hint revisions cannot
silently leave squares in dialogue. Update screenshots and documentation to the
implemented maps and prices.

Keep performance PR #3 and map PR #6 independently reviewable. Create a local
combined checkout after their commits and validate the shared C Host interaction,
static/pending rendering and official firmware build together. Preserve the
square display, official application/recovery contract and existing local TE
integration. No main-branch merge or device write is included. Map wire format
v4 rejects old v3 saves; do not erase user data or claim migration.

Acceptance: all ten floors geometrically connected; mandatory key/boss gates
cannot be bypassed; either branch on each fork works while the other reward
remains guarded; all four example campaigns win in real C and survive per-action
save/import; all authored non-ASCII UI characters have generated glyphs;
combined interactions, operation-driven render tests and official build pass.
Example-route success is not proof that every resource decision is safe.
