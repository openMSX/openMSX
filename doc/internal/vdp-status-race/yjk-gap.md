# V9958: the 4 pixels before a YJK picture

## The question

In the YJK modes the picture starts 4 pixels after the display start (`yjk-timing.md`).
What does the V9958 show in those 4 pixels: the border, with the backdrop colour of each
pixel's moment, or something else?

## The tests

On a Panasonic FS-A1GT and a Philips NMS 8250 with a V9958, with the programs of
`yjk-timing.md`. A backdrop (R#7) stripe whose left edge moves 1 Z80 cycle (1.5 pixels)
per group of 8 lines, through the display start and the 4 pixels after it, in SCREEN 8 and
SCREEN 12, R#18 = 08h:

- `vdpline5` in SCREEN 12 (`yjk-timing.md`);
- `vdpyjk2.bas` screens 2 and 3, `vdpyjk3.bas` screens 1 and 2: `VDPYJKL.BIN` (16 groups,
  the second write 22 Z80 cycles after the first) on a turboR, `VDPYJKM.BIN` (28) on other
  machines; TP set, so colour 0 is black and doesn't show the backdrop;
- `vdpyjk5.bas` screens 1 and 2: `vdpyjk3`'s screens 1 and 2 again, with a ruler; screen 2
  also as a video.

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The groups' writes are 1 Z80 cycle apart, at a fixed moment relative to HR | the programs, `README.md` | measured |
| The display start of SCREEN 8 is where the stripe edge reaches the first column | SCREEN 8 has no 4-pixel shift | by construction |

## The results

- **SCREEN 8:** the stripe edge moves on by about 1.5 pixels per group until it reaches the
  first column, the display start.
- **SCREEN 12:** the edge never stops inside the 4 pixels before the first column. At about
  the group where it reaches the display start in SCREEN 8, the stripe colour fills those
  4 pixels at once:
  - `vdpyjk5`, FS-A1GT: in SCREEN 8 the edge reaches the display start at group 4.6; in
    SCREEN 12 it stops 1.7 pixels before the display start, and the 4 pixels fill from
    group 4.7 on;
  - `vdpyjk3`: the fill comes after an edge at about 4.5 pixels before the first column
    on the NMS 8250, about 7 on the FS-A1GT (both about the display start of SCREEN 8,
    give or take a group); on the FS-A1GT in both runs;
  - `vdpline5`, FS-A1GT: group 6's 4 pixels flicker between black and the stripe colour,
    the group whose write comes around the display start (its clock drifts).

## Conclusions for the hardware

1. **The 4 pixels between the display start and the first YJK pixel show one colour: the
   backdrop as it is at the display start**, to within one Z80 cycle, not the backdrop of
   each pixel's moment.
2. *Interpretation, not measured:* the display window opens at its usual moment, the YJK
   pixels come out 4 pixels later (`yjk-timing.md`), and in between the VDP outputs the
   backdrop it took when the window opened.
3. Not measured with a masked border (R#25 bit 1), which covers those pixels.

## Files

- The programs are those of `yjk-timing.md`.
- The photos are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/yjk-gap/photos`.
