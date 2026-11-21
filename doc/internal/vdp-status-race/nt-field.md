# V99x8: an NT write in the middle of a field

## The question

NT (R#9 bit 1) selects 50 Hz (313 lines) or 60 Hz (262 lines). What happens to the field in
which it is written? Does the field keep the length it started with, or does it change,
and from which moment in the field is a write too late?

## The tests

`vdpnt.asm` (with `vdpnt.bas`, `vdpnt2.bas`, `vdpnt3.bas`), `vdpnt4.asm` (with
`vdpnt4.bas`); `autoexec-vdpnt.bas` runs whichever hasn't saved its result yet. Per call:
R#9 = NTA, 3 frames to settle; sync on VR's fall (the display start), count N lines (HR
pulses), wait, write R#9 = NTB, then count the lines to VR's next four edges (rise, fall,
rise, fall), to within one line. Interrupts off; SCREEN 0 WIDTH 80, 212 lines. Direction A
is 50 to 60 Hz (NTA = 82h, NTB = 80h), B is 60 to 50 Hz. Each setting twice.

- `vdpnt.bas`: N = 0 to 340 in steps of 2 to 50.
- `vdpnt2.bas`: around the end of the field: N = 249 to 257 (A) and 224 to 232 (B), with
  an extra wait DLY = 0 to 16 in steps of 2 (14 Z80 cycles per step; 16 steps is about a
  line).
- `vdpnt3.bas`: direction A around the end of a 60 Hz field: N = 199 to 206, DLY 0 to 16.
- `vdpnt4.bas`: around the switch of `vdpnt2` (N = 253 in A, 229 in B), a delay of 65 to
  125 Z80 cycles in steps of one, 6 times each, with R#18 = 00h, 07h and 08h.

Below, D = N + the first two counts: the lines from one display start to the next.

Machines: Panasonic FS-A1GT (V9958), Sony HB-F700D (V9938), Philips NMS 8250 with a
V9958. (`vdpnt.bas` ran on the first two with a version of `vdpnt.asm` without DLY, which
writes 32 Z80 cycles earlier.)

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| Counting HR pulses counts lines; VR's edges are the display's end and start | `flag-positions.md` | measured |
| `vdpnt4`'s delay counts from an HR edge, and HR moves with R#18 | `flag-positions.md` | measured |
| The display start takes NT at the line counter reset | `line-latch.md` | measured (the reset's moment), here only used for the top border |

## The results

The three machines agree to within one line (except one N where the NMS 8250's later write
lands in the next field).

**`vdpnt`**, D and the blanking lines that follow the next display:

| A (50 to 60 Hz), N | D | then |
|---|---|---|
| 0 to 200 | 234 | 48 |
| 205 to 252 | 484 | 48 |
| 254 to 266 | 285 | 48 |
| 270 to 312 | 312 | 21 |
| 315 to 340 | 547 | 48 |

| B (60 to 50 Hz), N | D | then |
|---|---|---|
| 0 to 225 | 339 | 99 |
| 230 to 245 | 288 | 99 |
| 248 to 260 | 261 | 126 |
| 262 to 340 | 601 | 99 |

Read as fields, direction A: a write up to N = 200 gives the field the 60 Hz length (D =
234 instead of 285); from N = 205 on the field runs on much longer (484: 250 lines more
than a 60 Hz field); from N = 254 on the write is too late for this field, which keeps the
50 Hz length; from N = 270 on it lands in the next field, which gets the 60 Hz length but
keeps the 50 Hz top border. Direction B: up to N = 225 the field gets the 50 Hz length
(339 instead of 288); from N = 230 on it is too late; from N = 248 on it lands in the next
field, which gets the 50 Hz length and keeps the 60 Hz top border.

**`vdpnt2`**, the switch (N and the first DLY with the new outcome): A at N = 253, DLY 14
on all three machines (DLY 12 mixed on the FS-A1GT); B at N = 229, DLY 14 on all three.
**`vdpnt3`**: from D = 234 to 484 at N = 202, DLY 14 on all three.

**`vdpnt4`**, the switch as the delay (Z80 cycles) at which half the trials have the new
outcome:

| R#18 | FS-A1GT, A / B | HB-F700D, A / B | NMS 8250, A / B |
|---|---|---|---|
| 00h | 101.0 / 100.8 | 102.5 / 103.0 | 105.3 / 102.2 |
| 07h | 105.2 / 106.7 | 108.5 / 105.8 | 109.2 / 108.7 |
| 08h | 94.8 / 96.3 | 94.7 / 98.7 | 97.7 / 97.5 |

From R#18 = 07h to 08h HR comes 60 VDP cycles (10 Z80 cycles) later, and the switch,
counted from HR, comes 7 to 14 Z80 cycles (about 10 to 11 on average) earlier; from 00h to
07h HR comes 28 VDP cycles earlier and the switch 3 to 7 Z80 cycles later. So the moment
that decides doesn't move with the adjust.

## Independent measurements: igmar, Philips VG-8235 (V9958)

igmar's test ROM (PR #2300; the ROM and its sources are attached to PR #2299), test M21
part B (steps 5 to 8): NT written (50 to 60 Hz) in one of the first lines of a field, with
R#18's high nibble (the vertical adjust) at 7, 3, F and 8; then the lines counted between
VR's edges, as `vdpnt` does. Each row is the line of the write and the count of blanking
lines that follows; every row then reads 213 / 49 / 213 / 49 / 213 (the new top border)
or 213 / 22 / 213 / 49 / 213 (the old one kept):

| step | R#18 | new top border | old top border kept |
|---|---|---|---|
| 5 | 70h | line 14 | lines 15, 21, 22 |
| 6 | 30h | line 14 | lines 15, 17, 18 |
| 7 | F0h | lines 13, 14 | line 15 |
| 8 | 80h | lines 6, 7, 14 | line 15 |

Against the tests above:
- With the new top border the blanking after the display is 49 lines, as in a 60 Hz field
  (`vdpnt`: 48, counted the same way to within one line). With the old (50 Hz) top border
  kept it is 22 lines: the field still ends at the 60 Hz length, with the longer top
  border in it. That is `vdpnt`'s case of a write early in the next field (direction A, N
  = 270 to 312: 21 blanking lines). So also here the field in which NT is written gets the
  new rate's length.
- The top border changes up to line 14 and is kept from line 15 on, with every vertical
  adjust tried: it is taken at the line counter reset, in line 14 here
  (`line-latch.md`).

## Conclusions for the hardware

1. **An NT write changes the length of the field it is made in.** NT is not latched per
   field.
2. A mechanism that fits all results: **the line counter counts up, and once per line it is
   compared with the last line of a field of the rate NT selects at that moment.**
   - In time: the field gets the new rate's length.
   - When the counter is already past the new rate's last line, there is no match: the
     field runs on until the counter wraps, at 512 lines.
   - Too late (the compare for the old rate's last line has already been made): the field
     keeps its length, and the next field starts with the new rate.
   - The top border is taken from NT at the line counter reset, which is why a write in the
     next field's first lines changes its length but not its top border.
3. **The moment of that compare**, fitted (`vdpnt2`, `vdpnt3`, `vdpnt4` on all three
   machines, run in openMSX with the timing of the other documents): a write is in time if
   it comes at least 400 ± 10 VDP cycles before the end of the line in which the field would
   end, i.e. up to 709 ± 10 VDP cycles after HR falls at the start of that line (R#18 =
   00h). `vdpnt2` and `vdpnt3` alone allow 360 to 520; with 400 the emulated switch of
   `vdpnt4` lies within 0.9 Z80 cycles of the real one on average per machine, with 380 or
   420 2.9 to 5 cycles off. The same moment holds for the end of the old rate (`vdpnt2`,
   `vdpnt4`) and for a new rate's end that would already have passed (`vdpnt3`).
4. **This moment doesn't move with the horizontal adjust**, unlike HR, F, FH, VR, the line
   latch and the line counter reset.
5. igmar's M21 on a third machine (below) agrees: an NT write in the first lines of a
   field changes that field's length, and its top border only up to the reset line.

## Files

- `vdpnt.asm`, `vdpnt.bas`, `vdpnt2.bas`, `vdpnt3.bas`, `vdpnt4.asm`, `vdpnt4.bas`,
  `autoexec-vdpnt.bas`. Assemble and run as in `README.md`.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/nt-field`: `<machine>-nt-{a,b}.txt` (`vdpnt`),
  `<machine>-nt2-{a,b}.txt` (`vdpnt2`), `<machine>-nt3.txt` (`vdpnt3`): N (and DLY), then
  the four counts; `<machine>-nt4-{a,b}.txt` (`vdpnt4`): R#18, N and the delay, then the
  four counts.
