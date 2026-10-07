# Makoto implementation notes

`MSXMakoto` exposes a YM2608 OPNA at ports 14h–17h. The board uses an 8 MHz
oscillator, direct MSX `/INT` wiring and 256 KiB ADPCM-B RAM in x1 DRAM mode.

The chip core lives in `src/sound/YM2608.{hh,cc}`. FM and ADPCM engines started
as YMFM by Aaron Giles (BSD-3-Clause; notice at the top of `YM2608.hh`) and are
now openMSX-owned code specialized for this device. SSG uses the existing
`AY8910` (YM2149) implementation. The six internal rhythm samples are in
`src/sound/YM2608AdpcmRom.hh` (libvgm/MAME fmopn reconstruction; GPL-2.0-or-later).

## Audio

FM/rhythm/ADPCM run as one stereo resampled device; SSG is a separate mono
device. Sample rates follow the chip prescaler (default 6 → about 55.6 kHz FM
and 250 kHz SSG). Prescale values 2 and 3 are accepted as on the chip but are
outside the usual datasheet operating point at 8 MHz.

FM/rhythm/ADPCM enter the floating-point mixer without an internal 16-bit clip;
final host clipping remains. Analogue distortion and pot taper are not modeled.
Device names derive from the MSX device ID so multiple cartridges can coexist.

## State and debugger

`Makoto ADPCM RAM` is a `Ram` blob (zero-filled at startup as emulator policy;
reset does not clear it). `Makoto registers` exposes effective core registers
with side-effect-free peeks. Register edits synchronize sound and keep the
program address latch. Save states and rewind use the normal openMSX path;
mixer rates are rebuilt from the restored prescaler.

## Hardware-checked details

Physical Makoto V4/V5 probing informed ADPCM-B CPU transfer boundaries (inclusive
END/LIMIT) and retained-buffer behavior when an unfinished CPU write is switched
to read mode. Exact EOS timing, beyond-end transfers and full bus timing are not
claimed as complete.
