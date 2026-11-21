# V99x8: the line latch and the line counter reset

## The question

Two things happen once per line at a fixed moment, and openMSX had both at the same moment,
144 VDP cycles into the line (issue #1310):
1. **The line latch.** The VDP takes the display mode, the display enable (R#1 BL) and
   the sprite disable for the coming line. A write that comes before it affects that line,
   a write after it only the next one.
2. **The line counter reset.** In one line per frame (line 8 + the vertical adjust), the
   display line counter is reset, and NT and LN of R#9 are taken for the next frame.

Where in the line is each of them, on the V9938 and the V9958? Are they the same moment?

## The tests

1. **`HINTTOBL`** (grauw's test program from issue #1310; `hintz80.asm` wraps it so that
   a turboR runs it in Z80 mode). SCREEN 1, in rows of 4 lines: each row waits for a line
   interrupt with `HALT`, then waits a fixed time, one Z80 cycle longer in every next row,
   and writes R#1 with the display disabled, enabled again 38 Z80 cycles later. As long as
   the disable comes before the latch, a line goes blank. The rows are labelled with their
   wait (Z80 cycles from the interrupt to the 11th cycle of the `OUT`); the reading is the
   last row that still has a blank line, even one that only flickers (`HALT` adds 0 to 4
   cycles).
2. **`vdplc`** (`vdplc.asm`, `autoexec-vdplc.bas`). The `vdpmode` trial of
   `flag-positions.md` (sync on VR, HR's phase), then a wait into the reset line of the
   next frame, where R#9 is written with LN = 0 (192 lines instead of 212) at a moment d, in
   steps of one Z80 cycle. Then S#2 is read about 5 lines after the display start of 212
   lines. The reset fixes the display start from LN as it is at that moment; with 192
   lines the display starts 10 lines later, so VR is still 1 there if the write came
   before the reset ("seen") and 0 if it came after it ("before"). Then LN = 1 again before
   the next frame's reset; each trial takes two frames. GRAPHIC4 and TEXT2, each with R#18
   = 00h, 07h and 08h, 8 trials per read moment.

Machines: Panasonic FS-A1GT (V9958), Sony HB-F700D (V9938); grauw ran `HINTTOBL` on a
Yamaha CX5MII (V9938).

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| `HINTTOBL` counts from the moment the CPU sees the line interrupt | the program | by construction |
| Where FH is set, and how long after it /INT goes active | `flag-positions.md`, `output-delay.md` | FH's position: fitted, to one Z80 cycle; /INT's delay: fitted, 24 (V9958) / 20 (V9938) VDP cycles. **The latch's fitted position depends on both.** |
| `vdplc`'s read moments relative to HR's falling edge, and HR's position per mode | `README.md`, `flag-positions.md` | measured, to one Z80 cycle |
| The display end compares the line counter with LN as it is at that moment, so only the display start shows when LN was taken | how R#9 LN works (the end can be changed during the frame) | known from the V9938 data book; the test relies on the start |

## The results

**`HINTTOBL`**, the last row with a blank line:

| machine | Z80 cycles |
|---|---|
| Yamaha CX5MII (V9938), grauw in #1310 | 242 |
| Sony HB-F700D (V9938) | 242 |
| Panasonic FS-A1GT (V9958) | 241 |

Software with writes near this moment, on the real machines: Woei's split (issue #470)
is stable on both. In the copper part of Dawn of Time (issue #1313) the top line is missing
now and then on the HB-F700D (1 or 2 times per second, by eye); on the FS-A1GT its bottom
line flickers.

**`vdplc`**, the last read moment e at which the write is taken and the first at which it
isn't (the reset lies in between):

| | GRAPHIC4 | TEXT2 |
|---|---|---|
| FS-A1GT, R#18 = 00h | 140\|141 | 135\|136 |
| FS-A1GT, R#18 = 07h | 140\|142 | 135\|136 |
| FS-A1GT, R#18 = 08h | 140\|141 | 135\|136 |
| HB-F700D, R#18 = 00h | 157\|160 | 152\|154 |
| HB-F700D, R#18 = 07h | 158\|162 | 154\|156 |
| HB-F700D, R#18 = 08h | 159\|160 | 153\|158 |

e is relative to HR's falling edge in the mode of the test, which is 31 VDP cycles (5 Z80
cycles) later in TEXT2 (`flag-positions.md`): so the reset is at the same moment in both
modes. It doesn't move relative to HR with R#18, so it moves with R#18 as HR does. (The
two machines' e can't be compared: their VDP I/O differs. On the HB-F700D, with locked
clocks, only some e get trials, so its edges are wider.)

## Independent measurements: igmar, Philips VG-8235 (V9958)

igmar measured where NT and LN are taken with his own test ROM (PR #2300; the ROM and its
sources are attached to PR #2299). The Z80 runs locked to the VDP (228 Z80 cycles a line)
from RAM, with interrupts off; L is the program's line reference from its lock on HR, and
T a Z80 cycle.
- **NT (test M21, GRAPHIC4, R#18 = 00h):** NT written in the reset line at L + n. 50 to 60
  Hz: the new rate's top border for n up to 211, the old one from n = 212 on; a pass in
  9-T steps and one in 1-T steps agree. 60 to 50 Hz: the same edge, 212. A write in the
  next line keeps the old border. With R#18's high nibble (the vertical adjust) at 7, 3, F
  and 8, the line in which NT is taken moves along with the reset line.
- **LN (test M15, two runs):** the LN change takes effect from +6 / +2 / +12 T from the
  reset line's HR, for R#18's low nibble 0 / 7 / 8 (another run: +7 at 0). In TEXT1 about
  +17 T.

Against `vdplc`:
- With the fitted reset of this document and the positions of `flag-positions.md`, his NT
  edge comes out at 211.8 T: one moment for NT, as `vdplc` measured for LN.
- The steps with R#18, -4 and +10 T, are those of the adjust (-4 VDP cycles a step,
  28 and 60 VDP cycles): the reset moves with R#18 as HR does, as `vdplc` shows. *This reads
  his reference as one that doesn't move with R#18, as in his test M13
  (`flag-positions.md`): an assumption, his description doesn't say.*
- *Not compared:* his TEXT1 edge, 11 T after the GRAPHIC4 one. `vdplc` measured TEXT2
  (80 columns) and found the same moment as in GRAPHIC4; whether 11 T fits depends on
  which HR edge his program locks on in TEXT1, which his description doesn't say. Open.

## Conclusions for the hardware

1. **The line latch**, fitted (`HINTTOBL` run in openMSX with the positions and delays of
   `flag-positions.md` and `output-delay.md`): 91 to 96 VDP cycles before HR falls at the
   start of the line it is for on the V9938, 79 to 84 on the V9958. *These rest on the
   fitted /INT delay of FH and on FH's fitted position, and the fit is a range because
   `HALT` makes the interrupt response vary. Why the two chips' latches would be 12 VDP
   cycles apart is not explained by a mechanism.*
2. **The line counter reset is a different moment.** Measured: the same moment in
   GRAPHIC4 and TEXT2, and it moves with R#18 as HR does. Fitted: 101 VDP cycles before HR
   falls, on both chips (4 VDP cycles either way fits worse): 5 to 22 VDP cycles before
   the latch.
3. **Independent checks of the reset:** IKA9958 (a V9958 rebuilt from die shots, see
   `flag-positions.md`) resets its display line counter (`vacntr`) at the moment its
   decoder counter is reloaded, and loads it there from NT and LN. With the counter offset
   of `flag-positions.md` that is 97 VDP cycles before HR falls, one counter step after
   the fitted 101, and it moves with R#18. igmar's measurements on a third machine (below)
   put the moment NT and LN are taken at the same place. IKA9958 has no per-line
   latch of BL, the M bits and the sprite disable (it uses them as they are), so it says
   nothing about the latch.

## Files

- `hintz80.asm`: needs `hinttobl.com` from issue #1310 next to it (download link in the
  file). `vdplc.asm`, `autoexec-vdplc.bas`. Assemble and run as in `README.md`.
- The raw results of `vdplc` are in https://github.com/MBilderbeek/vdp-measurements,
  folder `vdp-status-race/line-latch`: `<machine>-lc-{screen5,text}-r18-<0|7|8>.txt`,
  per e the number of trials (columns e, unused, seen, before).
