# Makoto experimental branch validation — 2026-10-05

The structural and silence-detection direction is supported by these tests.
This patch fixes integration regressions found in Wouter's experiment and
restores runnable validation for its new engine API and state layout.

Base: `openMSX/openMSX` `makoto-experiment`,
`99496177b41e02c50c43720eb56b8eaae7867914`.
Reference: PR #2209, `e34f671e61dbf7c2462b4432310315658ed4ae56`.
The validation fixes are published separately on `maxiwamoto/openMSX` branch
`codex/makoto-validation-2026-10-05`; the upstream experiment remains unmerged.

## Fixes

- Include `<numbers>` explicitly: the Windows build otherwise cannot resolve
  `std::numbers::sqrt2_v` in `YM2608.cc`.
- Use the requested SSG register when dispatching a register write. Preserve
  debugger address-selection effects (including prescaler addresses 2Dh–2Fh)
  while retaining the running program's address latch.
- Synchronize pending sound before a prescaler address write changes rates.
  Drive the reused AY8910/YM2149 SSG clock at the corresponding rate, and save
  and restore its sample clock alongside the FM clock.
- Give AY8910 a clock-frequency setter and preserve its sample-clock phase
  when output configuration is reapplied without changing the period.

Before the fixes, the debugger regression changed register 7 when asked to
edit register 8. A 976.6 Hz SSG tone stayed at 976.6 Hz through all prescaler
settings. Both regressions were reproduced in the compiled experimental build
(with only the missing include corrected). They pass after the fixes.
Debugger edits of all three prescaler addresses also match ordinary port
writes with either address bank selected.

Prescalers 2 and 3 at an 8 MHz chip clock are outside the specified Makoto
operating range. These tests preserve the emulator's existing modeled behavior;
they are not new hardware evidence for those rates.

## Engine and emulator checks

The updated `makoto-owned-core-test.cc` builds against either the previous
control layer or the experimental FM/ADPCM engines. It emits every source
sample of all 13 FM/rhythm/ADPCM voices. The comparison covers live writes,
LFO, FM algorithms, silent intervals and wake-up, closed pan, zero level,
ADPCM-B repeat/EOS/delta-N zero/CPU-fed data, channel masking, and CSM pulses.

All **246,940 stereo source frames** match exactly at each of four buffer
sizes: 1, 7, 127 and 4096. All 13 voices are required to produce sound.
Separate FM, rhythm and ADPCM negative controls produce 148,568, 21,552 and
49,380 differing output values respectively.

SSG is tested through the full emulator; it is no longer part of the
source-sample equality claim because the replacement uses a different volume
table. Pitch, channel isolation, mute, stereo routing, non-default prescalers,
exact state continuation and rewind checks pass. These checks do not establish
hardware amplitude calibration or a blind listening result.

The additional `makoto-ssg-compare.py` check records native 250 kHz channel
samples from both full emulators. All 16 envelope shapes, tone, envelope
period zero and envelope continuation after silence have identical level-index
sequences after amplitude-table normalization and sample-edge alignment.
Noise periods zero and one agree; both noise implementations exactly follow
the same full 131,071-step LFSR sequence, including after a silent interval.
The combined tone/noise output also matches its two expected generators.

There are real differences to keep explicit: AY8910 uses Galois state and YMFM
uses Fibonacci state, and initializing both encodings to 1 gives different
noise reset phases (17 noise steps apart). They are not sample-for-sample
identical after reset. The timed envelope-rewrite cases also differ at one
native sample (4 microseconds) at the register-write edge. These observations
are retained in the results; this patch does not change the noise seed or
claim that the new SSG output is bit-exact with the previous implementation.

Other passing checks:

- 40 ADPCM-B CPU transfer boundary cases and six retained-buffer/reset/state
  cases, including the existing hardware-derived V4/V5 expectations.
- Both timers, BUSY, IRQ acknowledge/cancellation, debugger peeks, save/load,
  native rewind and MSX reset.
- All six fixed rhythm samples, each channel mute, left/right pan, key-off and
  exact state continuation after restoring mid-cymbal.
- Two independent cartridges, separate RAM/registers/settings/IRQs, restore
  and removal of one instance.
- Native FM/operator/rhythm collection sizes, rejection of truncated
  collections and future state versions, and cleanup after failed sound-device
  registration.
- The portable listening script's eight cases, ADPCM-B left/right pan,
  single-shot EOS, repeat playback, exact save/restore and rewind continuation.

Full emulator build: Windows x64 Release, MSVC 14.50, using existing local
third-party libraries. Compiler warnings remain in upstream/vendor code.
No Linux/macOS build, new physical-hardware run or blind listening test is
claimed here. The experimental state layout remains a pre-merge format;
migration from published fork/experimental snapshots is not added.

## Performance

Five alternating-order pairs, Windows x64 Release, emulated Panasonic turbo R,
60 emulated seconds per case, dummy audio output with sound synthesis enabled.
These are median **whole-process CPU seconds**, not MSX driver execution time.

| Workload | Previous PR | Experiment + fixes | Change |
|---|---:|---:|---:|
| No Makoto | 3.281 | 3.406 | +3.8% |
| Idle Makoto in BASIC | 4.109 | 3.297 | -19.8% |
| Bustling Town | 5.984 | 5.094 | -14.9% |
| Music with channel tools | 6.516 | 5.422 | -16.8% |

All five pairs favored the experiment for the three Makoto workloads.
No-device runs varied substantially (3.125–4.047 seconds for the old build),
so these percentages are approximate. In particular, the candidate's slightly
lower idle-with-Makoto median than its no-device median is measurement noise;
it does not mean that adding a device makes emulation faster. Idle overhead
is now within the observed no-device variation.

The reference is the existing October 2 upstream Release test executable;
the candidate was built locally for this review. The branch also contains
intervening upstream changes, so this is a whole-build comparison rather than
an isolated attribution to each optimization. No compilation or other emulator
test was run concurrently with these measurements.

Executable SHA-256:

- Reference: `b2e3ef733b1e38af9dccf99ebb384ad539c10bf6650e3d0e73fe7143940ad0d0`.
- Candidate: `4f74fb4ead4fd97d3398ace1284bd2fd749f8fc7b9231f3b1928c87071145391`.

## What the 37-track ROM tests

ROM: `illusion_city_makoto_pc98_37.rom`.
SHA-256: `0342ca280afb3efeea765e37ef01d5aef4ddd406781296313fd9f52b8e853585`.

The player dispatches three FM channels and three SSG channels. Its source and
included listening instructions agree on this. A new I/O audit of 20 seconds
per track (plus the 0.1-second start key press) recorded **767,918 data writes**
across all 37 tracks. There were no writes to rhythm registers 10h–1Fh or to
ADPCM-B registers 100h–10Fh. No upper-bank FM channels were used either.

Consequently there is **no ADPCM track to select in this ROM**. Thunder (1),
Shop (4), Bustling Town (10) and Lao Shi (14) remain useful FM/SSG music cases.
The trace is an excerpt survey; the three-FM/three-SSG scope also follows from
the player's source, rather than from the excerpts alone.

## Portable ADPCM listening cases

Load `Contrib/makoto-adpcm-listen.tcl` with a Makoto-capable openMSX build:

```text
openmsx -ext Makoto -script Contrib/makoto-adpcm-listen.tcl
```

Open the console (F10) and enter `makoto_adpcm_test N`:

| N | Engine | Sound |
|---|---|---|
| 1 | ADPCM-A | Bass drum |
| 2 | ADPCM-A | Snare |
| 3 | ADPCM-A | Cymbal |
| 4 | ADPCM-A | Hi-hat |
| 5 | ADPCM-A | Tom |
| 6 | ADPCM-A | Rim shot |
| 7 | ADPCM-B | Generated 500 Hz RAM sample, once |
| 8 | ADPCM-B | Same sample repeating |
| 0 | Both | Stop |

For stereo routing, use `makoto_adpcm_test 8 left` and
`makoto_adpcm_test 8 right`. Case 8 continues until another case or case 0.
The script generates its own sample and uploads it through the chip's CPU
transfer interface. No music assets, external rhythm ROM or sample download
are needed. Use the ordinary sound-volume control if required.

## Reproduction

Python checks accept `--openmsx PATH --firmware-dir PATH`; NumPy is required by
the audio checks. Run `makoto-test.py`, `makoto-integration-test.py`,
`makoto-register-test.py`, `makoto-native-stream-test.py`,
`makoto-rhythm-test.py`, `makoto-instance-test.py`,
`makoto-state-layout-test.py`, `makoto-experiment-regression.py` and
`makoto-adpcm-listen-test.py` from `Contrib`.

`makoto-ssg-compare.py` instead accepts `--baseline PATH --candidate PATH
--firmware-dir PATH`. It factors out the volume table when comparing shape
indices, verifies noise against the full canonical sequence, and reports the
remaining reset-phase and rewrite-edge differences.

For source comparison, obtain the reference revision in a separate checkout.
Build the same current `Contrib/makoto-owned-core-test.cc` twice with C++23:

- Reference: define `MAKOTO_REFERENCE`; include reference `src`, `src/sound`
  and `src/3rdparty/ymfm`; compile reference `MakotoYM2608.cc`, `ymfm_opn.cc`,
  `ymfm_adpcm.cc` and `ymfm_ssg.cc`.
- Candidate: include current `src`, `src/utils` and `src/3rdparty/ymfm`;
  compile current `ymfm_opn.cc` and `ymfm_adpcm.cc`.
- Run `python Contrib/makoto-engine-compare.py --reference OLD_RENDERER
  --candidate NEW_RENDERER`.

Compile `makoto-adpcm-transfer-test.cc` with current `ymfm_adpcm.cc`, using
the same current include paths, and run it. It traverses native engine
serialization fields; full archive/rewind validation is performed by the
emulator checks above.

`makoto-track-coverage.py` and `makoto-workload-benchmark.py` additionally take
`--rom` and `--manifest`. The latter accepts repeated `--build LABEL=PATH`,
`--seconds 60 --repeats 5`. It alternates build order and tests no Makoto,
silent BASIC, Bustling Town and channel tools. Generated profiles and reports
are separate for every run.

Raw result files and build hashes accompany the review package.
