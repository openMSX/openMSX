# Makoto implementation notes

`MSXMakoto` integrates YMFM's YM2608 core at ports 14h-17h. The board uses an
8 MHz oscillator, direct MSX /INT wiring and 256 KiB ADPCM-B RAM in x1 DRAM mode.
Two independent native schedulables implement the chip timers. Time conversion
uses `Clock<8000000>`; the CPU BUSY interval follows the chip callback.

## Audio

MAX fidelity supplies a 1 MHz stream to the normal openMSX resampler. It is set
explicitly during construction and after loading core state (prescaler-derived
parameters are not all serialized by YMFM). Do not lower fidelity without
validating SSG edges and the channel-tap adapter.

YMFM's mixed FM/rhythm/ADPCM output already includes its shared DAC clamp. Its
combined SSG output applies integer 2/3 scaling; the raw SSG taps precede that
rounding. `MakotoMix` distributes the rounding remainder and shared attenuation
across separate voices. Attenuation is calculated only when the raw sum differs
from the clamped output. Floating-point summation order can differ slightly.
Channel-tool output is cached until FM/ADPCM or SSG inputs change.

Normal playback accumulates integer output bits to detect whole-buffer silence
without a per-sample conditional. Separate-channel output avoids silence scans.
Chip clocks continue even when downstream resampling is skipped. A muted but
active SSG may conservatively keep a combined buffer active.

The nominal FM:SSG ratio is 4.3:1 per YMFM LSB at maximum board settings. The
standard device volume controls the complete output; `makoto_psg_volume` adjusts
SSG balance. Analogue saturation, load response and pot taper are not modeled.

## State and debugger

The initial upstream sound-state version is 1. Use native versioning if the
pinned YMFM state layout later changes; do not infer compatibility from size
alone. Loading validates the core blob length. Loader and saver field order is
identical for in-memory rewind. Preview-fork migrations are not part of this
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
sample RAM transfers and percussion continuation. The standalone core and mix
tests check all voices and prescalers; the mix test also exercises clipping.
Use C++20 for the mix helper's sized spans.

Performance comparisons should include BASIC without Makoto, BASIC with silent
Makoto, actual music and individual channel tools. Use isolated profiles and
uninstrumented builds. Measure clipping separately before changing the DAC
attenuation path. Synthetic tests must retain deliberately clipped material.

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
