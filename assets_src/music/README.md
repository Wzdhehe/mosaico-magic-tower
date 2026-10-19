# Original fifty-floor soundtrack

Eight project-local loops, MIT licensed; no sampled recordings or external music assets. The existing title remains intact. The seven campaign scores are authored in `assets_src/level_design/campaign_content.py`; `tools/gen_campaign_music.py` deterministically encodes pitches, sixteenth rhythms, explicit bass, chords and AABA answers into `main/game_music_score.h`.

| ID | Area | Key | BPM | Bars | Loop seconds |
|---|---|---|---:|---:|---:|
| 0 | Title: Lantern at the Gate | G major | 96 | 16 | 40.000 |
| 1 | Broken Gate, 1–10F | D minor | 84 | 8 | 22.859 |
| 2 | Sunken Hall, 11–20F | D dorian | 72 | 8 | 26.667 |
| 3 | Ash Works, 21–30F | A minor | 96 | 8 | 20.000 |
| 4 | Silent Court, 31–40F | F major / borrowed minor | 68 | 8 | 28.235 |
| 5 | Star Crown, 41–49F | D minor | 88 | 8 | 21.819 |
| 6 | King, uncleared 50F | D harmonic minor | 104 | 8 | 18.464 |
| 7 | Home, after victory | D major | 72 | 8 | 26.667 |

Campaign phrases are two bars, arranged A/A/B/A. Four written chords each span a phrase; the explicit two-bar bass repeats. Gate is a sparse triangle phrase; Water is a short pluck; Forge has 3/1 sixteenth syncopation and a soft pulse; Court borrows B-flat minor; Crown and King reuse the minor-key identity, while Home answers in major. The waveform and gain choices are fixed offline tables.

`main/game_music.c` is the only synthesizer for both the device and WAV export: 24 kHz signed 16-bit mono, four integer voices (lead, two chord tones, bass), fixed 3072-sample echo, envelopes and gain/track fades. Render and setters allocate nothing. A host instance remains 6272 bytes. Mute outputs exact silence immediately while advancing the score; unmute continues at that position. Neither music nor idle sprites forces UI redraws.

Reproduce with:

```bash
python tools/gen_campaign_music.py --check
python tests/run_game_music.py
python tools/render_music.py
```

WAVs and metrics are written to `artifacts/music/`. Tests cover every track's clipping, DC, seam, sample slew, arbitrary chunk determinism, allocation failure, immediate mute and advancing timeline, exact authored notes and syncopation. Host CPU timings measure this algorithm on the computer; speaker quality, device stack margin, battery impact and subjective listening still need hardware acceptance.
