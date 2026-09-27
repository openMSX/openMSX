# Makoto / YM2608 prototype

This adds an experimental Makoto extension. Board details below were confirmed
by its designer; this is not a claim of fully hardware-validated emulation.

## Implemented

- I/O ports 14h–17h with both OPNA register banks and BUSY status.
- YM2608 FM, SSG and ADPCM engines from Aaron Giles's BSD-licensed YMFM.
- All six internal rhythm sounds included as a public sample reconstruction.
- Timer A/B events scheduled in emulated time, independently of host audio buffers.
- 256 KiB sample RAM (256K x 16 DRAM, low byte wired, x1-bit DRAM mode).
- Native openMSX stereo audio, recording, register inspection and save states.
- Standard `Makoto_volume` for the combined output, plus `makoto_psg_volume`
  for the physical SSG balance adjustment. The extra Master setting is removed.
- All 16 voices exposed to openMSX's existing mute/record/channel-viewer tools.
- A first-order analogue low-pass based on the 100k/47 pF summer feedback.
- Native debugger watchpoints and probe traces for I/O and IRQ analysis.

The pinned YMFM source and BSD license are under `src/3rdparty/ymfm`.
Only the OPN/SSG/ADPCM subset is vendored. Local patches initialize an operator
cache and expose individual channel output; see README.openmsx for exact scope
and the differential test against the original mixed path.

## Run

Add `-ext Makoto` when starting openMSX, or run `ext Makoto` in the console.
Use software written for the Makoto I/O ports. No game data is included;
internal percussion samples are built in.

Console controls:

```
set Makoto_volume 75
set makoto_psg_volume 50
soundlog start makoto.wav
soundlog stop
```

The ordinary sound-chip volume setting controls the final combined output.
The `Makoto registers` debuggable reads effective registers directly from YMFM.
It is not a log of the last bytes written: for example, a pending FM frequency
high byte takes effect only when the corresponding low byte is written. Use an
I/O trace when the original write sequence is needed. Reserved FM register
locations may expose YMFM's internal frequency-latch storage.

## Hardware details confirmed by Sander, 2026-09-26

The cartridge designer supplied the following details through the owner:

- 8.000 MHz oscillator on phi-M. At the normal prescaler, FM updates at
  8 MHz / 144 = 55.56 kHz. YMFM's maximum-fidelity output stream is 1 MHz
  (clock / 8), with an FM sample repeated 18 times. These rates are consistent;
  the emulator must not change the oscillator or slow the entire core to 55.56 kHz.
- YM2608 /IRQ pin 56 connects directly to MSX /INT, with a 10k pull-up.
  Timer A and Timer B can interrupt the CPU. This connection and the 8 MHz
  oscillator are fixed hardware properties, not extension options.
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

All six internal percussion sounds are available by default: bass drum, snare,
cymbal, hi-hat, tom and rim shot. The built-in 8192-byte ADPCM-A table is the
public chip-output reconstruction from libvgm's GPL-2.0-or-later FM core;
source revision, checksums and attribution are in
`src/3rdparty/ym2608/README.openmsx`. It is separate from the BSD-licensed YMFM
engine and requires no user-supplied sample file. This does not affect the
cartridge's separate 256 KiB ADPCM-B RAM.

The optional `<rom><filename>ym2608_adpcm_rom.bin</filename></rom>` entry still
overrides the built-in table and must contain exactly 8192 bytes. Older saved
machines with this entry continue to use their external ROM; those without it
now use the built-in samples. Core state and save-state version are unchanged.
The Illusion City opening tests use FM and SSG, so this change does not alter
their instruments or timing.

## Tracing

Use the native trace viewer instead of an extension `tracefile` setting.
For example, after inserting Makoto, run these console commands:

```tcl
set makoto_write_watch [debug watchpoint create -type write_io -address {0x14 0x17} -command {
    debug trace add Makoto.IO [format "%02X=%02X" [expr {$::wp_last_address & 255}] $::wp_last_value] -type string
}]
debug trace probe Makoto.IRQ
```

This records CPU port writes (including register-address selection) and IRQ
transitions on the same emulation-time axis as other openMSX traces. Inspect
`debug trace list Makoto.IO` or use the GUI trace viewer. Stop collecting with:

```tcl
debug watchpoint remove $makoto_write_watch
debug trace drop Makoto.IRQ
```

The I/O trace remains available until `debug trace drop Makoto.IO`. Limit capture
duration: tracing a polling or music loop can collect a large amount of data.
A `read_io` watchpoint can additionally record when the CPU polls status;
`wp_last_value` is only available for writes. Debugger reads are side-effect-free
peeks, so do not use them as a replacement for a real CPU read in a hardware test.

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

Makoto sound-state version 3 stores two native scheduled timers and uses the
ordinary openMSX class version to identify the pinned YMFM state layout.
Versions 1 and 2 shipped in public preview builds and still load: their absolute
timer deadlines are migrated without restarting the counters. Version 2's
`coreFormat` identifier is accepted only for its supported value (1).
Version 1 lacks filter and voice histories; they start at zero and refill at
the next FM update. Current states retain those histories.
A future core-layout change must bump the sound-state version and provide a
migration/legacy decoder rather than guessing compatibility from byte count.
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
The checks use synthetic programs, plus the public internal rhythm reconstruction;
no game assets are included.

## Timing cleanup, 2026-09-27

The board oscillator and IRQ wiring are now fixed in the device, as confirmed
by Sander. Timer A and B use independent native scheduled events. Timer/BUSY
intervals use `Clock<8000000>::duration()` instead of floating-point conversion.
The sound mix, sample rate, filters, volume defaults and YMFM core are unchanged.
Native class versioning replaces the separate core-format field for new saves;
public preview saves retain their pending timer deadlines when loaded.

Local validation covers both active timers across save/restore, stopping one
while the other runs, CPU IRQs, BUSY, reset, native trace watchpoints, channel
isolation, stereo and all four quarters of sample RAM. A released version-2
state and a derived version-1-layout fixture preserve exact pending deadlines,
core, BUSY, RAM and sample clock during migration. Unknown future versions and
unsupported legacy core formats are rejected. The core differential test still
matches 360,000 samples across all 16 voices and three prescalers.

Audio performance experiments remain separate from the timing/debugger cleanup.

## Debugger cleanup, 2026-09-27

Debugger I/O reads now use explicit YMFM peek methods. They inspect status
without propagating IRQs and inspect ADPCM transfer data without consuming the
two dummy reads, incrementing the sample address or changing EOS/BRDY flags.
SSG GPIO/override peeks use separate safe callbacks, never ordinary host reads.
Makoto's sample-memory callbacks are passive, and its SSG GPIO is unconnected.

The wrapper no longer saves and restores the entire YMFM core for each status
peek, suppresses IRQ callbacks, or maintains 512 shadow register bytes and a
second address latch. Register inspection uses core storage; later register-edit support is described below.
Normal CPU reads/writes, audio synthesis and the core's serialized layout are
unchanged. Version 4 saves omit the duplicate host registers/latch; versions 1–3
are read using their core state, with old duplicate fields consumed and ignored.

`Contrib/makoto-peek-test.cc` checks all four peek ports against real reads on a
cloned core across 1,189 states, including BUSY, GPIO, ADPCM dummy reads, RAM
address progression, EOS, wrap and playback. It verifies no core changes or
host read/write, IRQ or timer side effects. This is a synthetic software test,
not a new hardware validation of YM2608 behavior.

The rebuilt Windows fork passed native debugger, timer/IRQ, state-continuation,
channel, stereo and 256 KiB RAM tests. Repeated ADPCM data/status peeks preserve
the entire saved chip state. Version-2 and version-3 snapshots, plus a version-1
fixture derived from version 2, migrate with exact core/RAM/clock/deadline values.
The unchanged core differential test still matches 360,000 samples and the
1133-byte pinned state fingerprint. To test multiple older formats, repeat the
`--legacy-state` option of `makoto-integration-test.py`.

## Internal percussion, 2026-09-27

Makoto now supplies the public 8192-byte YM2608 rhythm reconstruction by default.
Previously the missing-ROM callback returned zero bytes; ADPCM zero codes are
not silence, so software triggering percussion could receive an incorrect
waveform. All six voices now receive their actual reconstructed sample data.
External 8192-byte rhythm ROMs remain supported, including in older snapshots.
The table is immutable and does not change the serialized core layout.

`Contrib/makoto-rhythm-test.py` verifies the pinned sample checksum and records
all six voices in an isolated emulator profile with no external rhythm ROM.
It checks per-voice muting, stereo pan, key-off, and exact full chip-state
continuation after restoring a snapshot taken during cymbal playback. It also
checks that an external ROM really overrides the table, its snapshot restores,
and an incorrectly sized override is rejected. This validates emulation paths;
it does not replace a percussion listening comparison with the physical board.

Run it with the same `--openmsx` and `--firmware-dir` arguments as the other
Makoto integration tests. No game ROM is required. Existing timer/IRQ, debugger,
FM/SSG, 256 KiB RAM and legacy-save tests also pass with the built-in table.

## Register editing and channel-tool optimization, 2026-09-27

`Makoto registers` now accepts debugger writes. These synchronize audio to the
current emulated time and use the normal YM2608 write path, including BUSY,
key-on, sample-RAM transfer and timer side effects. The program's address latch
is preserved, so an edit does not redirect the next CPU data-port write.
Prescaler register selection still changes the prescaler, just as real I/O does.
Register reads continue to show effective values, so FM frequency-pair latching
still applies. The running program may overwrite a manual edit on its next update.

Example: `debug write {Makoto registers} 8 15` sets SSG channel A amplitude.
`Contrib/makoto-register-test.py` compares edits against normal port writes for
SSG, both FM banks, ADPCM RAM, rhythm, timers and all three prescalers. Twenty
comparisons preserve exact complete chip state, including the selected bank.

When individual channel tools are active, the mixer now reuses channel targets
until the FM/ADPCM output or an SSG voice actually changes. Repeated samples
still run through the same filters. Sample rate, synthesis, gain, clipping and
save-state layout are unchanged; this is not a lower-fidelity rendering mode.

The paired Windows benchmark measured median process CPU time of 3.65625 s
before and 2.953125 s after for 20 emulated seconds with a channel muted: about
19% less CPU. Normal playback measured 1.828125 s and 1.8125 s, which is within
run variation. These are local synthetic workload results, not a universal
speedup and not a reduction in the emulated music driver's CPU requirements.

Both normal and separate-channel recordings matched the baseline exactly
(32,768 interleaved 16-bit PCM values per path), as did complete saved chip state.
The comparison changes prescalers and SSG gain during playback. Timer/IRQ,
percussion, debugger, 256 KiB RAM and legacy-state regression tests also pass.
Reproduce with `Contrib/makoto-benchmark.py --baseline ... --candidate ...
--firmware-dir ...`; see [raw results](makoto-performance-results.json).
The 1 MHz MAX-fidelity stream and 34 kHz summer filter remain in use. Broader
sample-rate or filter changes still need separate measurements and listening.

## Test-tool report safety

The rhythm test uses SHA-256 to identify the pinned sample data. Historical
CRC32/SHA1 values remain in the provenance note for comparison with upstream.
The benchmark creates `results.json` exclusively inside its fresh run directory
before launching the emulators and keeps that file handle through cleanup.
No CLI option selects the report path, and the final write does not re-resolve
it. `Contrib/makoto-tools-test.py` checks refusal to overwrite an existing or
linked file and report cleanup after a launch failure. As with any developer
test harness, executable arguments must point to builds the developer trusts.
