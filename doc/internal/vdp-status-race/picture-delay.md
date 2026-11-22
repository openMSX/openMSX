# V99x8: where a register write shows in the picture, relative to the status flags

## The question

The CPU times its writes from the status flags (HR, here). Where in the picture does a
register write show, relative to those flags? In particular: does the picture follow the
VDP's timing as the flags show it, or does it lag behind it? And is that the same on the
V9938 and the V9958?

## The tests

Both use the phase measurement of `README.md` (sync on VR, HR's phase measured per frame
with one HR sample per line, each one Z80 cycle later in the line).

1. **`vdpline3`** (`vdpline3.asm`, `vdpline3.bas`, `autoexec-vdpline3.bas`): a
   backdrop-colour stripe, written to R#7 at the same moment relative to HR's falling edge
   in each of 64 lines (a loop of exactly 228 Z80 cycles per line, so on a machine with
   locked clocks the stripe's edges are vertical), in SCREEN 5, text 80 and text 40.
   Above and below the stripe the program draws marker lines *in the picture*, in the
   VDP's own pixel grid, at the columns where the stripe's first and last pixel would be
   for a lag of 24 VDP cycles (solid) and of 33 (dotted), with the display area placed
   relative to HR as openMSX places it. A lag moves a write's effect to the *left*: when
   the write happens, the beam still shows pixels belonging to an earlier moment. Where
   the stripe's edge lies between the marker lines gives the lag. The autoexec first runs
   `clktest` (`README.md`), and the program counts per frame how many of the HR samples
   saw HR = 1 (its phase measurement), for the correction below. The machines' screens were
   filmed or photographed, the frames enlarged, and the marker lines and the stripe's edges
   marked by eye, at the top and the bottom of the stripe, on the left and the right edge.
2. **`vdpfh`** (`vdpfh.asm`, `vdpfh.bas`): the same stripe, but written by a line
   interrupt handler a fixed time after the interrupt (the CPU waits in `HALT`, IM2, no
   BIOS code), SCREEN 5. Markers: red = no lag for either the picture or the interrupt
   line, green = the picture 33 VDP cycles late and the interrupt line not.

Machines: Panasonic FS-A1GT (V9958), Sony HB-F700D (V9938), and for `vdpline3` a Philips
NMS 8250 with a V9958 instead of its V9938.

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The write moments relative to HR's falling edge | `README.md` (the phase measurement), `flag-positions.md` (where HR is) | measured, to one Z80 cycle |
| A register write takes effect in the VDP at the moment of the write | nothing contradicts it; a later effect would move the stripe to the *right*, the opposite of what is seen | assumed |
| On the NMS 8250 the CPU runs 116 to 117 ppm fast against the VDP: its writes land early, the more so the longer after the HR phase measurement | `clktest`, run again before `vdpline3` (117 ppm; FS-A1GT -3 ppm, HB-F700D 0) | measured; corrected for, see below |
| Where the display area starts relative to HR, which places the marker lines | openMSX's current placement, used to compute them | **assumed**; the photos measure the sum of that place and the lag |
| For `vdpfh`: the interrupt line's delay | `irq-delay.md` | fitted |

## The results

**`vdpline3`**, the lag read from the stripe's edges against the marker lines, in VDP
cycles (mean of the top and bottom of the stripe):

| | left edge | right edge | mean |
|---|---|---|---|
| FS-A1GT, text 80 | 29.2 | 30.5 | 29.8 |
| NMS 8250, text 80 | 29.0 | 29.7 | 30.5 (corrected, see below) |
| HB-F700D, text 80 | 31.4 | 26.4 | 28.9 |
| HB-F700D, SCREEN 5 | 31.8 | 27.9 | 29.8 |
| HB-F700D, text 40 | 31.6 | 23.9 | 27.7 |

- On the HB-F700D (a white stripe on black, because of its video output) the bright stripe
  looks wider than it is, so its left edge reads more lag and its right edge less; their
  mean cancels that. Text 40 has its marker lines only 2 pixels apart, so it weighs least.
- On the NMS 8250 the stripe slants (about 5 pixels in text 80 over its 64 lines) and its
  edges wobble, from its fast CPU. Its marker lines were placed for that: the writes land
  early by 116 ppm of the CPU cycles from the HR edge in the phase samples to the write,
  about 10 VDP cycles at the stripe's top and 21 at its bottom in text mode, 11 and 21 in
  SCREEN 5 (from the counted phase samples, which vary by one between frames, and 117 ppm).
  The marker lines assumed 1.2 VDP cycles more in text mode, so the reading is corrected
  by that.

**`vdpfh`**: on both machines the stripe lies on the red markers, at display x 57 to 227
on the FS-A1GT (wobbling a few pixels: its `HALT` ends on a 5-cycle grid) and 55 to 222 on
the HB-F700D.

The photos, videos and raw counts are in the data repository (see Files).

## Conclusions for the hardware

1. **Relative to the flags, a write shows about 29 to 30 VDP cycles further left in the
   picture than with the display area where openMSX places it, the same on all three
   machines and in all three modes** (`vdpline3`: 28.9 to 30.5). This holds although
   `flag-positions.md` moves HR's falling edge, the edge the test syncs on, by 33 VDP
   cycles relative to the display area in the graphics modes and not at all in the text
   modes: the offset is between the flags and the picture, not between HR and the picture
   in one mode.
2. **What it is, is open.** The photos can't tell a delay in the chip between its timing
   and its picture output from a display area that lies about 29 VDP cycles later relative
   to the flags than openMSX places it. A delay in taking the write would move the stripe
   the other way. Not settled here; the place of the display fetches among the VRAM access
   slots, relative to the flags, would tell.
3. **`vdpfh` agrees** with that offset together with the interrupt line's delay
   (`irq-delay.md`): relative to the line interrupt, a write lands about where a model
   without either puts it. With 29 and 22 the stripe lies on the FS-A1GT's photo and 3
   pixels left of the HB-F700D's; with 30 it lies 3 pixels left of both.

## Files

- `vdpline.asm` (the stripe loop) and `vdpline3.asm` (the same with the phase count; writes
  `VDPLIN3.BIN`), `vdpline3.bas`, `autoexec-vdpline3.bas` (`clktest`, then
  `vdpline3.bas`); `vdpfh.asm`, `vdpfh.bas`. Assemble and run as in `README.md`.
  `vdpline3.bas` holds the marker columns for a machine without the extra VDP I/O wait and
  with locked clocks, and for a turboR; the NMS 8250's were moved as described above.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/output-delay`: `vdpline3/` (per machine the `clktest` result, the phase
  counts, the photos or videos, the enlarged frames with the marks) and `photos/`
  (`vdpfh`).
