# Roadmap

## Current PR: handcrafted map design pass

- Rework floors 3–10 without increasing the 11×11 viewport or drawing load
- Preserve initial tutorial/merchant screens and their existing UI coverage
- Force floor 4 blue gate -> floor 3 red vault -> floor 5 red seal detour
- Place floor 7 red key behind a wraith for floor 8's seal
- Offer a floor 9 choice between two mandatory guardians
- Write hints into existing short, bilingual NPC dialogues
- Add structural gate/connectedness checks, re-record the full playable route
- Bump save format because the old stored map no longer matches the level geometry

## Next: review, balancing, performance

- Playtest alternative routes on real hardware, record boss HP left and key spend
- Measure hardware temperature, frame time and battery use before changing FPS
- Consider a lower-power render mode with correct animation/input timings
- Larger maps with a fixed-size camera viewport and on-screen pan arrows (separate feature)

## Later

- Extra story branches and score/challenge goals only if real playtests justify them
- Per-floor atmosphere tweaks, optional minimap and additional boss arrangements
