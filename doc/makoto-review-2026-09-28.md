# Makoto review follow-up, 2026-09-28

This batch addresses the mixing, silence-detection, state-format and rhythm-ROM
configuration review on upstream PR #2209.

## Audio paths and clipping

Normal playback retains YMFM's efficient combined output. `chip.generate()`
already returns the FM/rhythm/ADPCM sum after its shared 16-bit DAC clamp, and
the SSG sum after integer 2/3 scaling. `chip.ssg_output()` exposes the three raw
SSG voices before that rounding. These are different stages of the same signal.

The extracted `MakotoMix` helper is used by channel tools. It assigns the SSG
rounding remainder to an active SSG voice, and proportionally distributes the
shared DAC attenuation over the FM/rhythm/ADPCM taps. Independently clipping
each voice would change the result. No chip synthesis or clock rate is changed.

A standalone test uses the production helper and a separate native YMFM mixed
reference. It exercises all 16 voices, all three prescalers, three SSG gains,
360,000 samples, and 430,704 clipped sample/side/gain cases. Maximum difference
between the summed floating-point contributions and native combined mix was
0.0117188 chip units; the test limit is 0.03125. Float sums are not universally
bit-identical under arbitrary grouping. It also checks opposite-sign voice
cancellation and silence. The native YMFM outputs and core states match exactly.
The original core regression retains its 1,133-byte state fingerprint 4faf7577.

At the recorded 16-bit PCM level, the synthetic prescaler/gain-change sequence
matched exactly: 32,768 values per mode between the release and candidate, and
32,768 values between the candidate's combined and channel-tool paths. This is
a fixture result, not a claim that every possible waveform must be bit-identical.

## Silence detection and performance

Separate-channel rendering no longer maintains 16 audible flags or checks both
sides of every voice on every sample. It checks whole-chip silence only when
cached inputs change, and stops checking once any voice is audible in the buffer.
It checks individual contributions there because opposite voices can cancel in
the combined sum. The core always continues clocking. Silent individual channels
in an otherwise active buffer are no longer separately marked null.

Three alternating paired runs of 30 emulated seconds, using process CPU time:

| Path | Release median CPU seconds | Candidate | Reduction |
|---|---:|---:|---:|
| Normal combined playback | 4.671875 | 4.546875 | 2.7% |
| Channel tools, one voice muted | 9.515625 | 7.515625 | 21.0% |

Baseline: Windows release 2026.09.27.1. Both executables used the same fixture,
firmware, settings and dummy audio. These are synthetic host CPU measurements;
the small normal-playback difference is subject to run-to-run variation. They do
not make an overloaded emulated Z80 replayer faster. A first experiment routing
normal playback through all separated contributions increased normal CPU cost by
about 12% and was discarded.

Final raw benchmark: `derived/makoto-bench-3rqk9kgc/results.json`.
Earlier interrupted benchmark runs are excluded. Callback diagnostics and earlier
rescheduling were added to the test harness after control timeouts occurred with
both released and candidate executables. The final run completed successfully.

## State formats

The upstream candidate uses initial sound-state version 1 with identical field
order for saving and loading. Fork migration branches are absent there. It was
compiled as a separate Makoto object in the fork test harness, not as a complete
standalone upstream distribution.

Our fork retains version 5 and its released-save migrations. Obsolete register,
latch and filter XML fields are skipped by the XML loader rather than explicitly
read into discarded arrays. Real released v1, v2 and v4 saves preserve core, RAM,
IRQ, BUSY, sample clock and pending timer deadlines; available voice caches are
also preserved. Fork preview formats are not advertised as upstream-compatible.

Both candidates pass disk save/load continuation, native in-memory rewind with
active timers, future-version rejection, timer/IRQ/BUSY/reset checks, debugger
peeks, stereo/channel isolation, and CPU readback in all four 64 KiB quarters of
sample RAM. The upstream future-version test targets Makoto's sound element
specifically; other devices also have unversioned sound elements.

## Rhythm-ROM configuration

Upstream always uses the fixed built-in YM2608 rhythm reconstruction. The
external ROM loader, size check, optional pointer and XML setting are removed:
these samples are internal to the chip, not a user-replaceable Makoto ROM.
Our fork retains the override and its compatibility tests for future experiments.
The built-in samples themselves, playback gains and chip state layout are unchanged.
Both rhythm suites pass: all six voices, per-voice mute, stereo pan, key-off and
mid-cymbal save continuation. The fork also passes external override restoration
and invalid-size rejection; the rebuilt upstream device passes native rewind
and timer/IRQ/save-continuation checks.

## Reproduce

- Build `Contrib/makoto-core-test.cc` with YMFM's opn, ssg and adpcm cpp files.
- Build `Contrib/makoto-mix-test.cc` with the same files, adding `src/sound` to
  the include paths. Both standalone tests need C++17 or newer.
- Run `Contrib/makoto-test.py` and `Contrib/makoto-integration-test.py` with
  `--openmsx` and `--firmware-dir` against the corresponding tree/build.
- Run `Contrib/makoto-benchmark.py --baseline RELEASE --candidate CANDIDATE
  --firmware-dir FIRMWARE --seconds 30 --repeats 3` in the fork. It requires
  NumPy. WAV comparison stays exact between builds; the direct path comparison
  permits and reports at most one final PCM quantization step.

Benchmark state comparison ignores only unused SSG scratch slots 12–17 of the
host channel cache. Actual SSG state is compared in the core blob; FM/ADPCM voice
cache entries and all other saved chip fields remain checked.
