# Makoto filter comparison — 2026-09-27

The filter was removed after a paired performance comparison and one blind
listening session. The comparison candidate removes the cartridge summer's 4.7 us / 33.86 kHz one-pole filter.
It retains the 1 MHz chip stream, 8 MHz clock, gains, DAC clamp, YMFM synthesis,
resampling, and all channel tools. Its combined path no longer computes the
per-voice filter histories. This is a host performance change, not an increase
in emulated MSX or music-driver speed.

## Reproducible procedure

`Contrib/makoto-filter-test.py` takes `--baseline`, `--candidate` and
`--firmware-dir`; optional `--music-rom` adds local track 10/bank 5 from the
37-track Illusion City demo on Panasonic FS-A1GT (R800). The music ROM is not
included or uploaded. Both builds restore identical baseline machine states,
use equal pairwise mixer settings, and record through openMSX's normal output
resampler at 44.1 kHz. Recordings must have the same length and remain unclipped.

Audio fixtures: bright SSG square wave, repeated cymbal/hi-hat hits, and
Bustling Town. After each recording, core state, RAM, timer deadlines, IRQ,
BUSY, sample clock and FM/ADPCM voice caches match exactly. The old SSG entries
in the host channel-output array were mixer scratch space; the new combined
path does not populate them. SSG hardware state is compared inside the core.
Town's driver reports the same 10 late ticks in both recordings.

The CPU benchmark restores a synthetic six-FM/three-SSG fixture, warms up for
0.1 seconds, and measures 30 emulated seconds. Five pairs alternate execution
order. The individual-channel case mutes one FM voice to force separate buffers.
Windows process CPU time includes the whole headless emulator, not just Makoto.

| Path | Filter on median | Filter off median | Reduction |
| --- | ---: | ---: | ---: |
| Combined playback | 2.687500 s | 2.234375 s | 16.86% |
| Individual channel tools | 4.734375 s | 4.328125 s | 8.58% |

These are local synthetic-workload measurements, not a general game-speed claim.
Raw captures remain local; game audio is not included in this repository.

## Blind listening

Run `Contrib/makoto-blind-test.py <capture-directory>` after the capture completes.
It creates a self-contained offline HTML page with embedded, anonymous WAVs.
A/B identities are independently randomized per passage and stored only in a
separate `identity-key.json`, outside the page. Do not reveal that file before
listening. The page independently randomizes X for each of 12 trials, four per
passage, and shuffles their order. It provides no correctness feedback until
all trials finish. The score is X=A/B discrimination, not a sound preference.

Each pair receives identical cropping, common gain and 10 ms edge fades to
avoid loop clicks. There is no separate loudness normalization or time shift.
Source switching preserves position and uses an 8 ms crossfade. Volume should
remain unchanged within a trial. Answers persist locally; nothing is uploaded.
The page can export a result JSON. A 12-trial result alone cannot establish
inaudibility for every listener or material; repeated testing also changes the
interpretation of the reported single-test binomial probability.

`Contrib/makoto-blind-page-test.mjs <index.html>` checks the page logic using a
mock audio/DOM runtime: embedded WAV decoding, trial balance, synchronized
switching, listening gate, delayed scoring, persistence and binomial tail.
The browser automation runtime was unavailable during preparation. The listener
subsequently completed the test in the browser; this is not automated browser QA.

## Candidate validation

- `makoto-test.py`: CPU I/O, BUSY, independent timers/IRQ, debugger peeks,
  native watchpoints, reset and active-timer save/restore passed.
- `makoto-integration-test.py`: channel isolation/mute/stereo, 256 KiB RAM,
  passive peeks, save/restore and unknown-version rejection passed.
- Its paired filtered-baseline check measured the expected inverse filter
  response at 1 kHz and 13.889 kHz.
- `makoto-rhythm-test.py`: all six voices, pan/mute/key-off, mid-cymbal save
  continuation and existing external-ROM behavior passed.
- Blind-page logic tests passed; the completed listening result is below.

The experimental fork save version is 5; v2-v4 filter histories are read and
discarded, while the underlying chip state remains intact. The separate upstream
version-1 simplification requested by Wouter has not been mixed into this test.
An automated integration run timed out once in the file-based test harness;
the sequential repeat completed successfully on both builds.

## Completed blind result and decision

One listener completed one 12-trial session on 2026-09-27, with identities hidden
and no correctness feedback until the end:

| Passage | Correct matches |
| --- | ---: |
| Bustling Town | 2/4 |
| Bright PSG tone | 1/4 |
| Cymbal and hi-hat | 3/4 |
| **Total** | **6/12** |

Random guessing averages 6/12. The exact one-sided binomial probability of at
least six correct guesses is 0.61279296875 (61.28%). This session did not establish
reliable discrimination. It does not prove inaudibility, and four trials per
passage are too few for useful separate conclusions. ABX tests discrimination,
not which version sounds better. The anonymized trial results are in
[makoto-filter-listening-results.json](makoto-filter-listening-results.json).

Decision: remove the 33.86 kHz cartridge filter. The measured host CPU saving
is useful, while this test established no audible benefit from retaining it.
The normal host resampler remains active. Chip synthesis, clock, gain, timers
and emulated music-driver timing remain unchanged. This is a scoped performance
tradeoff, not a claim of exact analogue hardware equivalence.
