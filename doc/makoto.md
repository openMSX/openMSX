# Makoto / YM2608 prototype

This adds an experimental Makoto extension. Hardware details listed below are
still provisional; it is not a claim of fully hardware-validated emulation.

## Implemented

- I/O ports 14h–17h with both OPNA register banks and BUSY status.
- YM2608 FM, SSG and ADPCM engines from Aaron Giles's BSD-licensed YMFM.
- Timer A/B events scheduled in emulated time, independently of host audio buffers.
- 256 KiB sample RAM (capacity confirmed by the cartridge owner).
- Native openMSX stereo audio, recording, register inspection and save states.
- Separate `makoto_master_volume` (combined output) and `makoto_psg_volume` settings,
  each 0–100, defaulting to Master 50 and SSG 50. These follow the Master Vol. / SSG Vol. labels on the supplied PCB photo;
  the analogue calibration remains provisional.
- Optional CSV tracing of writes, status reads, timer overflows and IRQ changes.

The pinned YMFM source and BSD license are under `src/3rdparty/ymfm`.
Only the OPN/SSG/ADPCM subset is vendored, without modifications.

## Run

Add `-ext Makoto` when starting openMSX, or run `ext Makoto` in the console.
Use software written for the Makoto I/O ports. No game or internal rhythm ROM
is included.

Console controls:

```
set makoto_master_volume 50
set makoto_psg_volume 50
soundlog start makoto.wav
soundlog stop
```

The ordinary sound-chip volume setting controls the final combined output.
Register history is available as the `Makoto registers` debuggable.

## Hardware details still being checked

The current configuration uses a nominal 8 MHz clock. The owner has asked
SuperSoniqs for the schematic/technical details. IRQ is disconnected by default
until the current board revision is confirmed; chip timer flags still work for polling.
The older Project Makoto Final.pdf schematic (revision 4.22, dated 2020)
connects YM2608 IRQ to the MSX slot and places the master control after mixing.
The supplied 2025 PCB photos confirm Master Vol. / SSG Vol., YM2608B and
YM3016-D, but do not by themselves establish unchanged IRQ wiring or clock.
GPIO reads return FFh and are not connected to the MSX keyboard/joysticks.
RAM capacity is confirmed; addressing and ADPCM transfer behavior still need
hardware tests. FM/SSG balance is adjustable, not yet calibrated.

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
They do not establish correct analogue balance, sample RAM transfers or IRQ
wiring on current hardware. The designer has been contacted for more details.

Additional hardware references:
- https://github.com/denjhang/MSX-makoto-to-RE2-YM2608 (older Makoto schematic)
- https://github.com/denjhang/RE2-YM2608 (related board and chip documentation)

## Listening defaults update

Master 50 / SSG 50 is the latest owner-selected balance after testing on R800.
The earlier 50/30 setting partly compensated for overloaded Z80 playback. The extension mix gain is
192000 instead of the initial 12000: a common 16x (+24.08 dB) boost, preserving
the FM/SSG ratio. This is a listening default, not analogue hardware calibration.

## Background findings

[Playback/timing research](https://github.com/maxiwamoto/openMSX/blob/master/doc/makoto-replay-findings.md)
separates emulator behavior from CPU overload in the local music port. No game
assets are needed by the synthetic regression. New profiles default to 50/50;
existing saved settings take precedence.
