# Makoto implementation notes

The main body below records PR #2209 before `makoto-experiment`. The current
experiment replaces the SSG implementation and the engine state layout.
See [October 5 validation](../makoto-validation-2026-10-05.md) for current
architecture differences, runnable tests, measurements and limitations.

`MSXMakoto` integrates YMFM's YM2608 core at ports 14h-17h. The board uses an
8 MHz oscillator, direct MSX /INT wiring and 256 KiB ADPCM-B RAM in x1 DRAM mode.
Two independent native schedulables implement the chip timers. Time conversion
uses `Clock<8000000>`; the CPU BUSY interval follows the chip callback.

## Audio

The FM/rhythm/ADPCM stream runs at approximately 55.56 kHz stereo and SSG
at 250 kHz mono. The openMSX-owned YM2608 control layer clocks the FM/SSG/ADPCM
engines directly, without a repeated-sample generation path. All 16 voices remain
available. Both rates track the chip prescaler; prescalers 2/3 at 8 MHz are
outside the datasheet's specification and are not hardware-validated here.

FM/rhythm/ADPCM voices enter the floating-point mixer without internal 16-bit
clipping. Final host clipping remains. This intentionally differs for overdriven
signals. A 37-track survey of approximately 20 seconds per track found no clips;
it does not establish that all Makoto software is unclipped.

SSG voices are raw integers converted to float. The old per-sample gain,
integer 2/3 scaling and remainder redistribution are removed. Fixed normalization
(including 2/3 to preserve the prior audible balance) is folded into the device
amplification factor; user levels use standard device-volume controls. This
is a numerical normalization, not a claim of a physical 2/3 circuit gain. The
existing 4.3:1 board ratio and mono centre-pan compensation are retained.
This removes rounding of the old SSG sum, a difference below one old-source LSB.

Whole-buffer OR accumulation skips downstream work on silence while chip clocks
continue. FM and SSG have separate standard volume controls. Analogue distortion
and pot taper are not modeled. All names derive from the MSX device ID, so
multiple cartridges can coexist.

## State and debugger

The initial upstream sound-state version is 1. Use native versioning if the
pinned YMFM state layout later changes; do not infer compatibility from size
alone. Loading validates the core blob length. Loader and saver field order is
identical for in-memory rewind. FM and SSG clocks are both serialized; rates
and phases are reconstructed after restoring the core. Preview-fork migrations are not part of this
upstream device.

Sample storage uses `Ram`, exposing `Makoto ADPCM RAM` and using blob/delta
serialization. Startup uses zero-filled contents as deterministic emulator policy;
hardware power-on contents are unknown;
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

## Owned YM2608 control layer (2026-10-01)

MakotoYM2608 adapts the attributed YMFM control logic into openMSX code while
retaining its FM, SSG and ADPCM engines. Symmetric FmPart/SsgPart devices own
the clocks and resamplers. Each FM and ADPCM channel is generated for the
whole buffer before the next channel. A channel that is silent for the whole
buffer has its pointer cleared, which is also how separate and shared
destinations are summed. There is no retained output cache or repeated-sample
path. BUSY is passed explicitly to side-effect-free peeks.

The old patched wrapper is confined to Contrib/makoto-reference for differential
tests. The vendor ym2608 class is restored to its pinned revision; lower-engine
RAM corrections, cache initialization and debugger helpers remain documented.
No synthesis algorithm was rewritten. Zero-filled initial RAM is deterministic
emulator policy; hardware power-on RAM contents remain unknown.

The extra SSG trim is removed. Both streams use standard mixer volumes. To
retain a previous balance: new SSG volume = old SSG volume * old trim / 100.

The standalone comparison matched 1,080,000 samples covering all 16 voices and
prescalers 6/3/2. Four matched-state 20-second music excerpts matched exactly
at equivalent volumes. Five paired Windows runs measured about 7% less whole-
emulator CPU for music than the preceding native-stream experiment; this is
not an emulated MSX driver speedup. Silence still advances all chip engines.
See the fork's doc/makoto-owned-core-experiment.md and results JSON for raw
measurements, executable hashes and limitations.

This initial upstream state format remains version 1. Fork preview migrations
and optional external rhythm-ROM replacement remain in the fork.

## October 2 review cleanup

The owned control fields use native serialization, with separate fixed-layout
blobs for the four YMFM engines. Blob lengths are checked by the archive. The
upstream device has no packed legacy padding. Registration is owned by each
concrete audio part; failed construction is tested for both parts. The initial
clock period is zero, making a separate clock-initialized flag redundant.
SoundDevice::updateStream is exposed through Part::sync and updates all devices.

The OPNA template is explicitly instantiated in its vendor translation unit.
This fixes optimized GCC/Clang linkage; an isolated Clang link probe reproduced
the earlier failure and succeeds after the change. Vendor sources now use .cc
directly. The sound GUI no longer has any Makoto-specific changes.


Separate-channel rendering now uses independent FM, ADPCM-B and rhythm loops.
The standalone comparison still matches 1,080,000 samples. Five alternating
Windows benchmark repetitions showed no convincing speed gain: median music
CPU seconds were 11.359 before versus 11.203 after, and channel tools were
12.688 versus 12.813 per 60 emulated seconds. Run variation was larger than the
change, so this is a clarity improvement rather than a performance claim.

The optional merged-class experiment was also built and tested separately.
It preserved four 20-second recordings exactly and passed runtime state,
rewind, timer, RAM, channel and construction-failure tests. Its median host
cost was essentially unchanged, so it remains on the fork's
codex/makoto-merged-experiment branch for further discussion. Raw measurements
are in doc/makoto-review-2026-10-02-results.json on the fork cleanup branch.
