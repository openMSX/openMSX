# V9958: YJK moves the picture, not the timing

## The question

In the YJK modes (SCREEN 10 to 12, R#25 bit 3) the V9958's picture is known to lie 4
pixels further right than in SCREEN 8. openMSX did that by adding 4 to the horizontal
adjust (R#18), which moves *everything* with it: the picture, but also HR, F, FH, VR, the
line latch and the line counter reset. Is that what the V9958 does, or does only the
picture move? And what happens when R#25 switches YJK in the middle of a line?

The horizontal adjust itself, R#18, is the reference here: `flag-positions.md` showed that
the flags move with it, and below `vdplat` shows that the line latch does too.

## The tests

All on the V9958: Panasonic FS-A1GT and Philips NMS 8250 with a V9958. They use the
`vdpline` loop of `output-delay.md` (each frame synced to HR, then lines of exactly 228 Z80
cycles that write a VDP register at a fixed moment, in groups of 8 lines, each group 1 Z80
cycle later), photographed. `vdpline5.asm` is that loop for these tests; `genyjk.py`
generates its lines, and `vdpyjkl.asm`, `vdpyjkm.asm` and `vdpyjkn.asm` are variants with
16 groups (see their headers).

1. **`vdpyjk`** (`vdpyjk.asm`, `autoexec-vdpyjk.bas`): SCREEN 8 at the top, SCREEN 12
   from line 106 (switched by the line interrupt), with the same vertical lines drawn in
   both halves every 16 pixels. Does the picture move?
2. **`vdpline5`** (`vdpline5.asm`, `vdpline5.bas`): does the timing move? A backdrop (R#7)
   stripe whose edge lies in the left border, written at a fixed moment relative to HR,
   with HR's phase measured in the screen mode itself, next to pixel columns, in SCREEN 8
   and SCREEN 12. A backdrop change shows at a fixed place on the screen for a fixed write
   moment. If YJK moves HR along with the picture, the writes move along and the edge keeps
   its distance to the columns; if only the picture moves, the edge is 4 pixels further
   from the first column in SCREEN 12.
3. **`vdpmode`** (`flag-positions.md`) in SCREEN 12 (`autoexec-vdpmode3.bas`): F, FH, VR
   and HR relative to each other.
4. **`vdpyjk4.bas`**, **`vdplc2.bas`**: the R#23 staircase of `r23-groups.md`, the line
   latch (a staircase of R#1 writes: display off, on again 50 Z80 cycles later; a line
   stays black if its latch comes while the display is off) and the line counter reset
   (`vdplc` of `line-latch.md`), each in SCREEN 8 and SCREEN 12.
5. **`vdplat.bas`** (also on the Sony HB-F700D, V9938): the same R#1 staircase in SCREEN 8
   with R#18 = 00h, 04h and 0Ch, R#18 set before HR's phase is measured, so the writes move
   with HR. If the latch moves with R#18 as HR does, the black band is the same in all
   three.
6. **A mid-line YJK switch**: `vdpyjk2.bas` (screen 4), `vdpyjk3.bas` (screens 3 to 6,
   `vdpsw.asm`), `vdpyjk5.bas` (screens 3 to 6, `autoexec-vdpyjk5.bas`): R#25 = 08h and
   later 00h written in the middle of the line, and in SCREEN 5 an R#7 stripe written at
   the same moments, over columns every 8 pixels (`vdpyjk3`) or every 2 pixels with a
   ruler (`vdpyjk5`).
7. **Sprites and the right edge**: `vdpyjk2.bas` screen 1: in both halves of `vdpyjk`,
   sprites 3 pixels left of columns, and a column at x = 255.

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The writes are at the same moment relative to HR in every frame, 1 Z80 cycle apart per group | the programs, `README.md` | measured; on the NMS 8250 the groups are 1.2 pixels apart instead of 1.5, because its CPU runs 116 ppm fast (`clktest`) |
| A backdrop change shows on the screen at the place the beam is at that moment (plus the output delay, which is the same in both modes) | `output-delay.md` | measured for SCREEN 5 and text; assumed the same in SCREEN 8 and 12 |
| Distances read from photos | the columns in the picture as scale (16 pixels between columns) | about half a pixel; the NMS 8250's photos are read per row, the camera was slightly tilted |

## The results

- **`vdpyjk`:** the SCREEN 12 lines lie 4 pixels right of the SCREEN 8 lines.
- **`vdpline5`**, distance from the stripe edge to the first column, per group, in pixels:

  | group | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
  |---|---|---|---|---|---|---|---|---|
  | FS-A1GT, SCREEN 8 | 10.4 | 8.4 | 7.1 | 5.1 | 3.9 | 1.8 | 0.6 | 0 |
  | FS-A1GT, SCREEN 12 | 14.2 | 11.9 | 10.2 | 8.8 | 7.5 | 6.2 | 5.2 | 0 |
  | NMS 8250, SCREEN 8 | 16.6 | 15.5 | 14.4 | 13.3 | 11.5 | 10.3 | 9.2 | 8.2 |
  | NMS 8250, SCREEN 12 | 22.2 | 20.7 | 19.1 | 18.0 | 17.3 | 16.2 | 14.6 | 13.3 |

  In SCREEN 12 the edge is 4 to 6 pixels further from the first column. (Group 7 on the
  FS-A1GT reaches the first column in both: see `yjk-gap.md`.)
- **`vdpmode` in SCREEN 12** (FS-A1GT): F, FH, VR and HR at the same positions relative to
  each other as in SCREEN 8.
- **R#23 in YJK** (`vdpyjk4`): in both modes the seam lies on a ruler tick, in SCREEN 12 on
  the moved picture's grid, and it moves to the next tick at the same group in both modes.
- **The latch in YJK** (`vdpyjk4`), black lines: FS-A1GT 33 or 41 in both modes; NMS 8250
  66 or 74 in both. A latch 16 VDP cycles later would make the band about 21 lines longer.
- **The reset in YJK** (`vdplc2`), the edge in e: FS-A1GT 141|142 and NMS 8250 166|167, in
  SCREEN 8 and 12, with R#18 = 00h and 08h alike. 16 VDP cycles would be 2.7 Z80 cycles.
- **The latch with R#18** (`vdplat`), the black band: the same in all three screens on
  each machine, frame for frame (FS-A1GT 4 or 5 groups, HB-F700D 5, NMS 8250 mostly 8;
  the band's height differs per machine and isn't used).
- **A mid-line YJK switch**: YJK switches on and off where an R#7 write at the same moment
  lands, to within 1 to 2.5 pixels (`vdpyjk5`; FS-A1GT, top to bottom group: R#7 41.2 to
  129.1, YJK 38.7 to 129.1; NMS 8250, top group: 34.8 to 120, 33.6 to 120.1), and from
  that point on, in that line, the columns lie 4 pixels further right (`vdpyjk3`). The
  column right before the switch point shows red or orange for a group or two.
- **Sprites and the right edge**: in SCREEN 12 the sprites keep the same 3-pixel gap to
  the moved columns, and the column at x = 255 is there, 4 pixels further right.

## Independent measurements: igmar, Philips VG-8235 (V9958)

igmar's test ROM (PR #2299; the ROM, its sources and his measurements are attached there),
test W03 steps 12 to 19: an R#25 write in the middle of line 100 that turns YJK on (steps
12 to 15) or off (16 to 19), at about pixel 40, 100, 171 and 231, photographed in close-up
with 8-pixel bars, measured against lines 99 and 101 to about half a pixel (run 12):

| step | YJK | change at pixel | bars after it | right end | at the change |
|---|---|---|---|---|---|
| 12 | on | 40.0 | shifted (8k + 4), as line 101 | about 259.3 | dark, about 0.7 pixel |
| 13 | on | 100.4 | shifted | about 259.9 | black 100.4 to 102.0 |
| 14 | on | 171.3 | shifted | about 260 | near-white to 173.2 |
| 15 | on | 232.0 | shifted | about 259.8 | cyan to 233.1 |
| 16 | off | 42.1 | unshifted (8k), as line 101 | 256 | nothing visible |
| 17 | off | 101.9 | unshifted | 256 | none |
| 18 | off | 173.1 | unshifted | 256 | can't tell |
| 19 | off | 232.7 | unshifted | 256 | about 0.4 pixel |

His first run agrees for steps 14 to 19; steps 12 and 13 changed 1.1 to 1.5 pixels
earlier in run 12, which he doesn't explain.

Against the tests above: the 4-pixel shift and the right end change with the R#25 write
itself, in the middle of the line, in both directions, as `vdpyjk3` and `vdpyjk5` show. He
too sees a short transient where YJK is turned on; whether it is the same effect as the red
or orange column before the switch point here isn't known. A third machine, another
program, the same behaviour.

## Conclusions for the hardware

1. **YJK moves the picture 4 pixels, and nothing else**: not HR, F, FH or VR, not the line
   latch and not the line counter reset. R#18 moves all of these.
2. **The shift belongs to the pixel output, not to the timing**: a mid-line R#25 write
   starts and ends the shift where the write lands in the picture, in that line (here and
   in igmar's W03), as a
   change of the pixel path would; sprites and the right edge move along with the picture.
   A mid-line R#23 write takes effect at the same group as in SCREEN 8, on the moved grid:
   the data is fetched at the same moment, and comes out 4 pixels later.
3. *Interpretation, not measured:* the V9958 decodes YJK per group of 4 pixels, from 4
   bytes, so it can't output a group's first pixel before it has its fourth byte. A
   4-pixel delay in the YJK pixel path fits all of the above. IKA9958 (see
   `flag-positions.md`) reloads its horizontal counter from R#18 only, without a YJK term,
   which agrees with 1.

## Files

- `vdpyjk.asm`, `autoexec-vdpyjk.bas`; `vdpline5.asm`, `genyjk.py` (writes the `.inc` file
  `vdpline5.asm` includes: `yjk.inc`, or for the variants `yjkl.inc`, `yjkm.inc`,
  `yjkn.inc`, see the headers of `vdpyjkl.asm`, `vdpyjkm.asm`, `vdpyjkn.asm`),
  `vdpline5.bas`; `vdpsw.asm` (`vdpline4.asm` of `r23-groups.md` with `stairsw.inc`, from
  `genstair.py 29 stairsw.inc`); `vdpyjk2.bas` to `vdpyjk5.bas`, `autoexec-vdpyjk5.bas`;
  `vdplc2.bas`; `vdplat.bas`. Assemble and run as in `README.md`.
- The raw results are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/yjk-timing`: `fs-a1gt-mode-<MODE>-s12.txt` (`vdpmode`),
  `<machine>-lc-{screen8,screen12}-r18-<0|8>.txt` (`vdplc2`), and `photos/`.
