# V99x8: the interrupt line and the picture lag the status flags

## The question

The CPU sees the VDP's timing through the status flags (F, FH, HR, VR) and through the
moment its register and VRAM writes take effect. The VDP also has outputs: the interrupt
line (/INT) and the picture. Do those change at the same moment as the flags, or later?

Two observations in `README.md` already hint at a lag: in `irqtest` the interrupt sometimes
comes after a read that has seen *and cleared* F. If /INT followed F at once, that read
would have cleared /INT before the Z80 could sample it.

## The tests

All use the method of `README.md` (sync on VR, HR phase measured per trial, one read or
write at a chosen moment in steps of one Z80 cycle).

1. **`vdpirq`** (`vdpirq.asm`, `autoexec-vdpirq.bas`): the `vdprace` trial for F, with
   the interrupt enabled during the read A1 (IE0 on, `EI`, interrupt mode 2, a handler that
   reads S#0 and records that it ran and whether it saw F). Outcomes per read moment:
   - *before*: A1 doesn't see F, the handler does;
   - *lost*: neither sees F (the window of `README.md`);
   - *seen*: A1 sees F, no interrupt follows;
   - *pulse*: A1 sees F, and the interrupt still comes (the handler finds F cleared).

   Text mode and SCREEN 5, 32 trials per read moment.
2. **`vdpirqh`** (`vdpirqh.asm`, `autoexec-vdpirqh.bas`, `vdpirqh2.bas`): the same for FH
   and the line interrupt (line 191, IE1 on, IE0 off, the handler reads S#1).
   `vdpirqh2.bas` repeats it on the HB-F700D (text mode with 64 trials).
3. **`vdpline`** (`vdpline1.asm`, `vdpline.asm`, `vdpline.bas`, `vdpline2.bas`): a
   backdrop-colour stripe, written to R#7 at the same moment relative to HR in each of 64
   lines (a loop of exactly 228 Z80 cycles per line, so the stripe's edges are vertical).
   Above and below it the program draws markers *in the picture*, at the columns where
   the stripe would start and end for a given lag of the picture (none, or 33 VDP cycles).
   A lag moves a write's effect to the *left*: when the write happens, the beam still
   shows pixels belonging to an earlier moment. The VDP draws the markers in its own pixel
   grid, so a photo shows the stripe against each model. (The legends on the screen name
   the openMSX version each marker was computed from when the photos were taken; what
   they mean as a lag is as given here.)
4. **`vdpfh`** (`vdpfh.asm`, `vdpfh.bas`): the same stripe, but written by a line
   interrupt handler a fixed time after the interrupt (the CPU waits in `HALT`, IM2, no
   BIOS code), SCREEN 5. Markers: red = no lag for either output, green = the picture 33
   VDP cycles late and the interrupt line not.

Machines: Panasonic FS-A1GT (V9958) and Sony HB-F700D (V9938).

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The read moments, the "lost" window and how it maps to VDP cycles | `README.md` (`vdprace`) | measured, to one Z80 cycle |
| The Z80 samples /INT at the end of an instruction; a read clears F (and drops /INT) a fixed time after it starts | Z80 data sheet; the latch/clear model of `README.md` | data sheet; a model that fits |
| For the photos: where the display area starts relative to HR | openMSX's current placement, used to draw the markers | **assumed**; the photos only measure the sum of that place and the lag, see below |
| The loop that writes the stripe takes exactly one line (228 Z80 cycles) | the program; the stripe's edges are vertical on the photos | measured |

## The results

**`vdpirq`**, read moments (e, one Z80 cycle apart) of the outcomes from the window on:

| | lost | pulse | before again from |
|---|---|---|---|
| FS-A1GT, SCREEN 5 | 92-94 | 95-107 | 108 |
| FS-A1GT, text | 87-89 | 90-101 | 102 |
| HB-F700D, SCREEN 5 | 97-101 | 102-111 | 113 |
| HB-F700D, text | 92-96 | 97-106 | 107 |

Right after the lost window, a read always sees F and clears it, and the interrupt *still*
comes: about 13 Z80 cycles long on the FS-A1GT, 10 on the HB-F700D. The pulse range ends
where the interrupt comes before the read.

**`vdpirqh`**, the same for FH:

| | lost | pulse | before again from |
|---|---|---|---|
| FS-A1GT, SCREEN 5 | 42-45 | 46-57 | 58 |
| FS-A1GT, text | 32-35 | 36-47 | 48 |
| HB-F700D, SCREEN 5 | 47-51 | 52-62 | 63 |
| HB-F700D, SCREEN 5, second run | 48-52 | 53-63 | 64 |
| HB-F700D, text | 38-42 | 43-51 | 53 |
| HB-F700D, text, second run (64 trials) | 38-41 | 42-52 | 53 |

A pulse range too, ending 1 to 2 Z80 cycles earlier (relative to the lost window) than
F's.

**`vdpline`**, the stripe against the markers:

| screen | FS-A1GT | HB-F700D |
|---|---|---|
| SCREEN 5 | on the 33-cycle marker (at most 1 pixel right) | 2-3 pixels right of it |
| text 80 columns | on the 33-cycle marker; about 15 text pixels left of the no-lag marker | about 3 text pixels right of the 33-cycle marker; about 14 left of the no-lag one |
| text 40 columns | on the 33-cycle marker | about 1.5 pixels right of it |

**`vdpfh`**: on both machines the stripe lies on the red markers (no lag for either
output), at display x 57 to 227 on the FS-A1GT (wobbling a few pixels: its `HALT` ends on a
5-cycle grid and its clocks aren't locked) and 55 to 222 on the HB-F700D.

The photos and the raw counts are in the data repository (see Files).

## Conclusions for the hardware

1. **/INT lags F.** A read just after F is set sees and clears F, yet the interrupt
   comes later. So /INT goes active only some time after F is set: the length of the pulse
   range. Fitted to where it ends: **33 VDP cycles on the V9958, 24 on the V9938**.
2. **The picture lags the timing by the same amount.** `vdpline` independently puts the
   stripe where a 33-cycle lag puts it on the V9958, and 21 to 27 cycles on the V9938.
   Two different outputs, measured in two different ways, give the same lag per chip.
   That fits *one* fixed delay between the VDP's internal timing (flags, register and VRAM
   writes) and its outputs. *Weak point:* the photos measure the lag plus the place of the
   display area relative to HR; if that place were off, the agreement would be a
   coincidence. The interrupt test has no such dependence.
3. **/INT of FH lags FH less: about 24 VDP cycles on the V9958, about 20 on the V9938**
   (fitted: 21 to 24 and 19 to 20). Why FH's line would be faster than F's is not known.
   On the HB-F700D in text mode the pulse range ends 1 Z80 cycle earlier than any delay
   reproduces (both runs): *not explained*.
4. **`vdpfh` agrees:** relative to the line interrupt, a write lands where a model without
   any lag puts it, because both the interrupt and the picture lag. The few cycles by which
   FH's /INT is faster don't show: the CPU's interrupt response goes in whole
   instructions.

## Files

- `vdpirq.asm`, `autoexec-vdpirq.bas`; `vdpirqh.asm`, `autoexec-vdpirqh.bas`,
  `vdpirqh2.bas`; `vdpline1.asm` (writes `VDPLIN1.BIN`), `vdpline.asm`, `vdpline.bas`,
  `vdpline2.bas`; `vdpfh.asm`, `vdpfh.bas`. Assemble and run as in `README.md`.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/output-delay`: `<machine>-irq-{text,screen5}.txt` (`vdpirq`: per e the
  number of trials lost, seen, before, pulse), `<machine>-irqh-*.txt` and
  `hb-f700d-irqh2-*.txt` (`vdpirqh`, the same columns), and `photos/` (`vdpline`,
  `vdpfh`).
