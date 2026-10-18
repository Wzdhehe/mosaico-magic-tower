# Original Magic Tower score

Three original, project-local 4/4 loops composed for this game on 2026-10-18.
No external samples, borrowed melodies, recordings or runtime music libraries
are used. The note source is `main/game_music_score.h`; it contains every note,
rest, tie, chord, rhythm, voice level, waveform and note phase increment.
The score and renderer carry the project's MIT license.

| Track | Title | Character | Key | Tempo | Bars | Loop |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | Lantern at the Gate | Warm arrival; a rising question and settled answer | G major | 96 BPM | 16 | 40.000 s |
| 1 | The Stone Remembers | Spacious exploration; descending replies and a leading-tone turn | D minor | 100 BPM | 16 | 38.400 s |
| 2 | Wind on the Stair | Forward motion; short rests, syncopated answers and climbing high points | E minor | approximately 108 BPM | 16 | 35.557 s |

The upper track uses 108 instead of the initial approximate 112 target so a
16-bar loop remains longer than 35 seconds. Its eighth is rounded to 6667
samples (107.995 BPM). These are separate compositions, not a transposed motif.
Each has an A phrase (bars 1-8), a contrasting B phrase (9-16), breathing space
at cadences, and a final tonic that leaves room for the first note to return.

Title progression: G, Em, C, D / Am, Bm, C, D / G, D/F#, Em, Bm / C, Am, D, G.
The melody opens B-D-E-D, then answers lower; the B section revisits its opening
with a different landing, rises on C, and closes F#-A-D-C-A-F# into G.

Lower progression: Dm, Bb, Gm, A / Dm, C, Bb, A / Gm, Dm, Eb, A / Dm, Bb, A, Dm.
Longer held notes and silence replace the title's regular answers. C# over A
supplies the minor-key pull; Eb in bar 11 briefly darkens the B section before
returning through A to D.

Upper progression: Em, C, Am, B / Em, D, C, B / G, D, Am, B / C, Am, B, Em.
Its repeated-note pickup and root/fifth bass rhythm give urgency without a
continuous high arpeggio. D# over B points toward E. Bar 9 opens higher on G,
then the final phrase descends and releases before the loop restart.

## Sound and limits

The portable renderer `main/game_music.c` is the *only* DSP implementation used
by the ESP service and previews. Output is 24,000 Hz, signed 16-bit mono. Four
fixed voices provide melody, two quiet inner harmony tones and bass. The bass
is raised one octave above the stored harmonic root to place its fundamentals
roughly 117-370 Hz, where a small speaker has more useful output. All voices
use a restrained odd-harmonic, exactly antisymmetric 256-entry table:

    round(32700 * (sin(2*pi*i/256) + 0.10*sin(6*pi*i/256)) / 1.1)

This avoids the old asymmetric triangle's DC error and harsh high pulse voice.
Note increments are generated offline as
`round(440 * 2**((midi-69)/12) * 2**32 / 24000)`; render does no frequency
calculation, division or floating-point work per sample. Notes receive about
5.3 ms attack and 42.7 ms release, with additional short articulation gaps.
Two softer chord tones change once per bar. The one-tap echo is fixed at 3072
samples/128 ms, with 1/4 wet level and no feedback or unbounded tail.

Nonzero volume changes ramp by at most 64/32768 per sample (at most about
21.3 ms for a full-scale change). Track changes fade the old track down, clear
its echo/voices, restart the selected score, and fade up (about 10.7 ms each).
Setting volume to zero means exact silence immediately; it clears the echo.
While muted, the renderer advances score/oscillator state by note-sized spans,
without generating voices, applying envelopes or processing echo. Unmute ramps
up at the current musical position, with a fresh echo history. Tests show its
score/phase matches continuous rendering once the gain and echo settle.

The opaque instance owns 6272 bytes on the current x64 host ABI (almost all the
3072-sample echo). Actual ESP layout can differ slightly. One `calloc` occurs
at create and one `free` at delete; there is no allocation in render or setters.
All score/wave tables are immutable read-only data. No render-local aggregate
exceeds 128 bytes. The ESP adapter separately allocates its one service context,
including the reusable 480-byte output block and board codec configuration.
The successful ESP service runs until application reset, preserving the public
`game_audio_init/set_volume/set_track` API. Only its audio task owns the core;
UI controls and startup guard use C11 atomics, not volatile shared variables.
Startup allocation/task failures free all application-owned state. The BSP owns
and caches the speaker codec/I2S handle: a failed codec open is closed but the
BSP handle is never deleted into a dangling cached reference. BSP internals
and their own allocation-failure handling are outside this module's ownership.

## Reproduce and audition

From the repository root, with a C11 compiler available:

    python tests/run_game_music.py
    python tools/render_music.py

On Windows, set `CC` to your MinGW `gcc.exe` or add its directory to `PATH`. Both scripts put compiler temporary files in the local
`artifacts/tmp` directory. No device, ESP-IDF install or extra Python package is
needed. The test runner uses a test-only allocation-symbol substitution so it
can force the real core's initial `calloc` to fail and check safe NULL output.
The normal preview build uses the unchanged production allocator.

The preview script compiles `main/game_music.c` into a temporary local shared
library, calls its public C API in 240-frame blocks, and writes standard WAV
headers. Python does not synthesize or alter PCM. Default previews use volume
100 to make headroom and signal level easy to inspect; the game defaults to45.
Use `--volume 45` to hear the game level or `--loops 2` to audition loop joins.
Output (ignored build artifacts):

- `artifacts/music/title-lantern-at-the-gate.wav`
- `artifacts/music/lower-the-stone-remembers.wav`
- `artifacts/music/upper-wind-on-the-stair.wav`
- `artifacts/music/preview-metrics.json`
- `artifacts/music/red.txt` and `green.txt` (TDD/runtime evidence)

The initial failing test was `FAIL line 25: a`: the deliberately empty core
could not create a valid handle. The full implementation then passes argument
and forced-allocation-failure handling, independent-instance control/deletion,
determinism, arbitrary chunk boundaries, immediate mute, advancing mute phase,
non-silence, distinct tracks, 35-45 second loop lengths, signal/DC/clipping,
loop-seam bounds and volume/track transition continuity checks.

Current full-volume results with the 5× makeup gain and 100% codec output:
peaks 29580/26970/27880, RMS about 10725/9618/9671 and mean sample values
-13.67/-13.26/-14.79 (out of 32768). No clipping occurs on the shipped scores.
The maximum adjacent-sample jump is 5535, proportional to the higher peak. The
mixer clamps to the int16 range as a safety net; all arithmetic stays inside
signed 32-bit limits.

One observed optimized x64 host test rendered all 113.96 seconds of music in
0.078 CPU seconds; 120 seconds of muted timeline was below the host clock's
1 ms resolution. These are host test observations, **not measured ESP CPU,
real-time margin, thermal, speaker-quality or subjective listening claims**.
Auditioning the exported WAVs and listening on the actual hardware remain the
appropriate subjective checks. Full ESP integration build is performed by the
parent integration task.
