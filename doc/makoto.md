# Makoto / YM2608 prototype

This adds an experimental Makoto extension. Board details below were confirmed
by its designer; this is not a claim of fully hardware-validated emulation.

## Implemented

- I/O ports 14h–17h with both OPNA register banks and BUSY status.
- YM2608 FM, SSG and ADPCM engines from Aaron Giles's BSD-licensed YMFM.
- Timer A/B events scheduled in emulated time, independently of host audio buffers.
- 256 KiB sample RAM (256K x 16 DRAM, low byte wired, x1-bit DRAM mode).
- Native openMSX stereo audio, recording, register inspection and save states.
- Standard `Makoto_volume` for the combined output, plus `makoto_psg_volume`
  for the physical SSG balance adjustment. The extra Master setting is removed.
- All 16 voices exposed to openMSX's existing mute/record/channel-viewer tools.
- A first-order analogue low-pass based on the 100k/47 pF summer feedback.
- Optional CSV tracing of writes, status reads, timer overflows and IRQ changes.

The pinned YMFM source and BSD license are under `src/3rdparty/ymfm`.
Only the OPN/SSG/ADPCM subset is vendored. Local patches initialize an operator
cache and expose individual channel output; see README.openmsx for exact scope
and the differential test against the original mixed path.

## Run

Add `-ext Makoto` when starting openMSX, or run `ext Makoto` in the console.
Use software written for the Makoto I/O ports. No game or internal rhythm ROM
is included.

Console controls:

```
set Makoto_volume 75
set makoto_psg_volume 50
soundlog start makoto.wav
soundlog stop
```

The ordinary sound-chip volume setting controls the final combined output.
Register history is available as the `Makoto registers` debuggable.

## Hardware details confirmed by Sander, 2026-09-26

The cartridge designer supplied the following details through the owner:

- 8.000 MHz oscillator on phi-M. At the normal prescaler, FM updates at
  8 MHz / 144 = 55.56 kHz. YMFM's maximum-fidelity output stream is 1 MHz
  (clock / 8), with an FM sample repeated 18 times. These rates are consistent;
  the emulator must not change the oscillator or slow the entire core to 55.56 kHz.
- YM2608 /IRQ pin 56 connects directly to MSX /INT, with a 10k pull-up.
  Timer A and Timer B can interrupt the CPU. IRQ is now connected by default.
- 256 KB ADPCM-B RAM: 256K x 16 DRAM with only the low byte used, x1-bit
  DRAM mode. Software selects this mode through the chip registers; the
  existing YMFM address shift is two bits in x1 mode. The cartridge RAM
  callbacks mask addresses to 18 bits.
- I/O is 14h-17h. Software should poll status 1 at 16h, because reads at
  14h conflict with the Music Module on real hardware. Makoto still responds
  at 14h: the collision is a shared-bus issue, not a missing chip register.
- YM3016 FM/ADPCM/rhythm feeds each inverting summer through 20k, with
  100k feedback (gain -5) and 47 pF (approximately 34 kHz low-pass corner).
- Mono SSG passes a 1k/1k divider and the SSG Vol dual-10k pot, whose gangs
  are paralleled, then feeds both summers (approximately x2.17 at maximum).
  Sander gives the resulting ratio as 4.3:1 FM:SSG per YMFM LSB with both
  pots fully up. The emulator therefore mixes each FM stereo output with
  `(SSG / 4.3) * ssgGain`, then applies the standard device volume to both together.
- Master Vol is a dual-10k pot after the summers. The headphone path uses
  a TPA6111A2 (x1.23), then 100 uF coupling to the jack. Each channel also
  feeds MSX SOUNDIN through 7.5k.
- Rev 1.1 uses LMV358; Rev 1.2 uses NE5532. Pot taper is unspecified.

The implementation models the confirmed digital wiring, relative gain and
summer filter. The filter uses tau = 100k * 47 pF = 4.7 us (33.86 kHz corner),
with an exact exponential step response at the 1 MHz core output rate. Filtering
the individual voices is equivalent to filtering their sum because the filter
is linear; this also allows the ordinary channel tools to work. The shared DAC
clamp is applied before the filter, preserving the original mixed signal.

The amplifier's constant x1.23 is absorbed into listening normalization; output
samples are not calibrated jack voltages. We do not invent board-revision
clipping/slew behaviour, headphone coupling response or host SOUNDIN mixing.
These depend on supply rails, load impedance and measured transfer functions.
The unspecified pot taper means slider percentages are linear gain, not dial
position. LMV358 and NE5532 are therefore documented hardware variants, not
unverified selectable distortion presets.
GPIO reads return FFh and are not connected to the MSX keyboard/joysticks.
ADPCM transfer behavior still needs comparison against the physical board.

YM2608's internal percussion needs its separate 8192-byte rhythm ROM. It is
not supplied with this branch. The extension accepts an optional `<rom>` entry
with filename `ym2608_adpcm_rom.bin`; without it, rhythm samples are silent.
The Illusion City opening tests use FM and SSG and do not need that ROM.

## Trace format

Copy Makoto.xml to a private extension, and add a `tracefile` element inside
`Makoto` naming a local CSV output file. Each row has:

```
ticks,event,address,value
```

One second is 3579545 * 960 ticks. Events are `write` (register 0–511), `read`
(port offset 0–3), `timer` (A=0/B=1), `irq` and `reset`. Values are decimal.
Tracing is off by default and can produce large files during polling.
Use separate trace files per run. Do not restore a state with tracing enabled:
the restored extension currently opens the same trace filename again.

## Verification

`Contrib/makoto-test.py` exercises BUSY, both timers, cancellation, debugger
peeks and save-state continuation using a synthetic cartridge and isolated
profile. It takes `--openmsx` and `--firmware-dir` arguments. No game assets
are part of the test or fork.

The private Illusion City project additionally captures the unchanged native
ROM's writes and WAV output and compares musical writes against the original
PC-98 game capture. It excludes GPIO/timer setup differences, aligns on the
same musical events and accounts for 7.9872 MHz versus 8 MHz clock rates.
Matching values do not establish matching timing or analogue sound balance.

## Sources

- https://github.com/aaronsgiles/ymfm (revision 81aec25ccbb98f4873a255f7551ac4dadac59b4a)
- https://map.grauw.nl/resources/msx_io_ports.php (Makoto port allocation)
- https://supersoniqs.com/ (hardware description)

## Validation status

The synthetic regression checks BUSY, timer A/B, cancellation, side-effect-free
debugger peeks, chip/timer save-state continuation and reset. It creates its own
ROM and isolated profile. Example:

```
python Contrib/makoto-test.py --openmsx /path/to/openmsx --firmware-dir /path/to/systemroms
```

The test needs Philips NMS 8250 firmware files; see the helper for filenames.
A Windows MSVC build on the development fork passed these tests. The PR-only
checkout is separately checked for compilation; this does not claim a complete
clean build on every platform. Host resampler buffers are not serialized, so
sample-identical WAV continuation immediately after restore is not guaranteed.

Private FM/SSG music comparisons matched original musical register sequences.
They do not establish full analogue fidelity or validate sample RAM transfers
on real hardware. The designer has now confirmed the wiring and nominal gain.

Additional hardware references:
- https://github.com/denjhang/MSX-makoto-to-RE2-YM2608 (older Makoto schematic)
- https://github.com/denjhang/RE2-YM2608 (related board and chip documentation)

## Volume controls and defaults

The standard Makoto device slider is now Master. New profiles use openMSX's
usual device volume of 75, and SSG defaults to 50. Configuration normalization
is 96000 (half the earlier 192000), absorbing the removed 50% custom Master
setting, so the default overall level is preserved before the new filter.
Existing `Makoto_volume` preferences remain effective. Scripts using the removed
`makoto_master_volume` must use `Makoto_volume` instead. In the general case,
new device volume = old device volume * old custom Master / 50; values above
100 require adjusting overall listening normalization instead.

The prior hardware update reduced SSG by 4.3 (12.67 dB) relative to the original
provisional mix. This ratio remains unchanged. The local 37-track demo launcher
has been updated for the standard volume setting.

## Background findings

[Playback/timing research](https://github.com/maxiwamoto/openMSX/blob/master/doc/makoto-replay-findings.md)
separates emulator behavior from CPU overload in the local music port. No game
assets are needed by the synthetic regression. New profiles default to device volume 75 and SSG 50;
existing saved settings take precedence.

## Hardware update validation, 2026-09-26

Timer IRQs, BUSY, peeks, reset and state continuation passed. Core differential
testing matched 360,000 samples and chip states across all three prescalers,
exercising all 16 voices. The filter measurement agrees with the component
calculation at 1 kHz and 13.89 kHz. Channel muting, stereo, CPU-driven sample RAM
in all four 64KB quarters, RAM restoration and an older state all passed.

The unchanged 37-track demo was checked on Turbo-R with thunder and both opening
themes. Register values and timestamps matched the previous build exactly.
The existing late-tick counts (2, 1, 1 in these excerpts) did not change; this
emulator work does not claim to eliminate music-driver deadlines. See the
[numerical results](makoto-hardware-results.json). Windows targeted compilation
passed in both the combined fork and the PR checkout; non-Windows builds have
not been run locally. This is not a new real-board audio comparison.

## Channel map, silence and state compatibility

| openMSX channel | Voice |
|---|---|
| 1-6 | FM 1-6 |
| 7-9 | SSG A-C (mono, fed to both summers) |
| 10 | ADPCM-B |
| 11-16 | Bass drum, snare, cymbal, hi-hat, tom, rim shot |

Normal playback filters the combined output and advances individual histories
analytically between input changes. Separate-channel tools use per-voice filtering.
Silent voices skip filter arithmetic and buffer writes; empty buffers are marked
silent so openMSX can bypass downstream mixing/resampling work. Chip clocks,
envelopes, noise and ADPCM continue running. This is a conservative host-side
optimization and does not fix late updates caused by an overloaded emulated Z80.

Makoto sound-state version 2 adds filter history, channel-output caches and an
explicit core-format identifier. Version 1 states still load; the added histories
start at zero and refill at the next FM update, so a short sound transient can
occur. Current states retain those histories. Core format 1 remains the pinned
1133-byte YMFM layout. An unknown format is rejected explicitly; a future core
upgrade must provide a migration/legacy decoder rather than guessing by size.
The core regression pins a deterministic old-format state fingerprint as well
as checking exact mixed-output equivalence. Host resampler buffers remain outside
the device state, so immediate WAV continuity after restore is not guaranteed.

## Reproducing the checks

`Contrib/makoto-test.py` covers timers, CPU IRQs, peeks, reset and save/restore.
`Contrib/makoto-integration-test.py` additionally tests channel isolation, stereo,
CPU-driven x1-mode RAM transfers and optional older-state/filter comparisons.
It requires NumPy; use `--openmsx` and `--firmware-dir`, with optional
`--legacy-state` and `--baseline` (the earlier gain-corrected binary). Both
executables use the same extension configuration for the filter comparison.

`Contrib/makoto-core-test.cc` needs no MSX firmware. Compile it as C++17 or newer
with `src/3rdparty/ymfm/ymfm_opn.cpp`, `ymfm_ssg.cpp`, `ymfm_adpcm.cpp`, and that
directory on the include path. It exercises all voices with synthetic data.
No game assets or rhythm-ROM contents are included in these checks.
