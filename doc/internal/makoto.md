# Makoto implementation notes

`MSXMakoto` integrates YMFM's YM2608 core at ports 14h-17h. The board uses an
8 MHz oscillator, direct MSX /INT wiring and 256 KiB ADPCM-B RAM in x1 DRAM mode.
Two independent native schedulables implement the chip timers. Time conversion
uses `Clock<8000000>`; the CPU BUSY interval follows the chip callback.

## Audio

The FM/rhythm/ADPCM stream runs at approximately 55.56 kHz stereo and SSG
at 250 kHz mono. A small YMFM subclass clocks the engines through their protected
interfaces, bypassing repeated samples in `generate()`. All 16 voices remain
available. Both rates track the chip prescaler; prescalers 2/3 at 8 MHz are
outside the datasheet's specification and are not hardware-validated here.

FM/rhythm/ADPCM voices enter the floating-point mixer without internal 16-bit
clipping. Final host clipping remains. This intentionally differs for overdriven
signals. A 37-track survey of approximately 20 seconds per track found no clips;
it does not establish that all Makoto software is unclipped.

SSG voices are raw integers converted to float. The old per-sample gain,
integer 2/3 scaling and remainder redistribution are removed. Fixed normalization
(including 2/3 to preserve the prior audible balance) is folded into the device
amplification factor; the configurable trim uses `setSoftwareVolume()`. This
is a numerical normalization, not a claim of a physical 2/3 circuit gain. The
existing 4.3:1 board ratio and mono centre-pan compensation are retained.
This removes rounding of the old SSG sum, a difference below one old-source LSB.

Whole-buffer OR accumulation skips downstream work on silence while chip clocks
continue. FM and SSG have separate standard volume controls; the additional
`Makoto_psg_volume` trim defaults to 50%. Analogue distortion and pot taper are
not modeled. All names derive from the MSX device ID, so multiple cartridges
can coexist. The GUI locates the trim separately for each instance.

## State and debugger

The initial upstream sound-state version is 1. Use native versioning if the
pinned YMFM state layout later changes; do not infer compatibility from size
alone. Loading validates the core blob length. Loader and saver field order is
identical for in-memory rewind. FM and SSG clocks are both serialized; rates
and phases are reconstructed after restoring the core. Preview-fork migrations are not part of this
upstream device.

Sample storage uses `Ram`, exposing `Makoto ADPCM RAM` and using blob/delta
serialization. Startup retains the integration's existing zero-filled contents;
reset does not clear sample RAM. Pause playback before editing sample RAM.
`Makoto registers` exposes effective core registers, with side-effect-free peeks.
Register edits synchronize sound and preserve the program's address latch.

The six rhythm samples are fixed chip-internal data reconstructed by libvgm.
Their separate license and provenance are in `src/3rdparty/ym2608/README.openmsx`.
The BSD YMFM core and local adapter changes are documented separately in
`src/3rdparty/ymfm/README.openmsx`.

## Validation

`Contrib/makoto-test.py` exercises timers, BUSY, IRQs, peeks, reset, save/load and
native rewind. Integration and rhythm tests cover channel controls, stereo,
sample RAM transfers and percussion continuation. Native core comparison covers
all voices and prescalers; the unclipped-mix test exercises deliberate overdrive.
The instance test covers duplicate extensions, independent IRQs, RAM and restore.
The prescale-restore test verifies derived configuration after loading a fresh core.

Performance comparisons should include BASIC without Makoto, BASIC with silent
Makoto, actual music and individual channel tools. Use isolated profiles and
uninstrumented builds. Measure clipping separately from performance. Synthetic
tests retain deliberately overdriven material.

Physical Makoto V4/V5 results validate the tested reset and x1 transfer-boundary
sequences. This does not establish all bus timing or beyond-end behavior.

### ADPCM-B CPU transfer boundaries

A physical Makoto V5 probe on Sanyo MSX2+ and Panasonic turbo R verified x1
END=0000 stores four bytes and LIMIT=0000 wraps after four bytes. The local YMFM
CPU read/write fix handles these inclusive boundaries without changing playback
helpers. `Contrib/makoto-adpcm-transfer-test.cc` covers these
and additional software boundary cases. Exact EOS timing, beyond-end transfers,
and other interrupted mode transitions remain outside this targeted fix;
the tests do not claim to model the complete transfer state machine.

### Unfinished CPU writer

Physical V4 testing found that switching an unfinished writer through mode 00h
to read mode returns the last CPU buffer byte until explicit ADPCM RESET. A
separate experiment against YM2608-LLE revision
7a2aca7b6830b96e48e3a4e1a40d15525993fa60 corroborates this retained-buffer behavior:
https://github.com/nukeykt/YM2608-LLE . That reference was used for investigation
only and is not incorporated into YMFM.

The core retains this state in a Boolean latch, appended to the YM2608 save
layout (1134 bytes total). Reads and peeks preserve the buffer until reset;
write completion or new playback also releases the latch. The original 1133
synthesis-state bytes are unchanged. Transfer regression covers reset recovery,
peeks and save/load; both V4 and V5 match hardware on two emulated host types.
Fork versions 1-6 migrate with a false latch only in the fork. The initial
upstream format includes it. Other interrupted transitions and exact bus timing
are not claimed as fully modeled.

## Review measurements

The 2026-09-30 follow-up records clipping counts, five paired CPU runs,
matched-state audio comparisons, and the fresh-core restoration regression:
https://github.com/maxiwamoto/openMSX/blob/codex/makoto-native-streams/doc/makoto-review-2026-09-30.md

The incremental simplification measured about 6% lower whole-process CPU with
channel tools. Normal playback changed by about 1%, within observed variation.
Matched-state 20-second excerpts of four tracks differ by at most one PCM count.
