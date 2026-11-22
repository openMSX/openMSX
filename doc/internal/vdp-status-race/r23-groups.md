# V99x8: a vertical scroll (R#23) write in the middle of a line

## The question

In the bitmap modes, the CPU can write R#23 (vertical scroll) in the middle of a line. From
which pixel on does the new value show? Pixel by pixel, from the moment of the write, or in
steps?

## The test

`vdpline4.asm` with `vdpline4.bas`, SCREEN 5. It is the `vdpline` loop of
`picture-delay.md` (each frame synced to HR as in `README.md`, then lines of exactly 228 Z80
cycles), but it writes R#23 instead of R#7: 8 at a chosen moment in the line, and 0 later
in the same line. The picture is blocks of 8 white and 8 black lines, so a scroll by 8
swaps them, and each write shows as a *seam* in the line where the blocks swap.

The 64 lines are unrolled in 8 groups of 8 lines (`genstair.py` generates them); in each
next group the first write comes 1 Z80 cycle (6 VDP cycles, 1.5 pixels) later. If a write
took effect from the pixel at the beam, the seams would form a smooth staircase, 1.5
pixels per group. A ruler drawn by the VDP above and below the picture has a tick every 8
pixels.

Machines: Panasonic FS-A1GT (V9958), Sony HB-F700D (V9938). Photos.

## What it rests on

| starting point | how we know it | how reliable |
|---|---|---|
| The writes are at the same moment relative to HR in every frame, 1 Z80 cycle apart per group | the program (exact loop lengths) and the HR phase of `README.md` | measured: the seams stay put on the HB-F700D, whose clocks are locked |
| The ruler marks the VDP's own 8-pixel grid | the VDP draws it in the picture | by construction |

## The results

The seam doesn't move 1.5 pixels per group: it **jumps by 8 pixels, always to a ruler
tick**. The seam at display x 136, 144 or 152, from which group on:

| | at 144 from group | at 152 from group |
|---|---|---|
| FS-A1GT | 0 or 1 (group 0 flips between the two) | 5 or 6 (group 5 flips) |
| HB-F700D | 1 | 6 |

On the FS-A1GT, whose clocks aren't locked, the writes of groups 0 and 5 sit right on a
boundary and flip between two positions. 8 pixels are 32 VDP cycles, about 5 Z80 cycles:
the seam moves one tick every 5 groups.

The photos are in the data repository (see Files).

## Conclusions for the hardware

1. **A mid-line R#23 write takes effect per 8-pixel group of the picture**, on the
   picture's own 8-pixel grid. That fits the way the VDP fetches the display data: in bursts
   per group of 8 pixels, ahead of the beam, so the scroll value is taken once per group,
   when its burst is fetched.
2. **Which group:** the first one whose burst comes after the write. Where those group
   boundaries are relative to the VDP's timing follows from where the picture is: the
   picture lags the timing (`picture-delay.md`), and so do its groups.
   How far ahead of its group a burst is fetched is fitted: the new value shows from the
   first group of the picture that starts at least 26 VDP cycles (24 to 27 fit) after the
   write, with the picture's lag of `picture-delay.md`. *Fitted,
   not derived from the fetch timing.*

## Files

- `vdpline4.asm`, `vdpline4.bas`, `genstair.py`. `genstair.py` writes `stair.inc`, which
  `vdpline4.asm` includes; then assemble and run as in `README.md`.
- The photos are in https://github.com/MBilderbeek/vdp-measurements, folder
  `vdp-status-race/r23-groups/photos`: `hb-f700d-vdpline4-stair.jpg`,
  `fs-a1gt-vdpline4-stair-1.jpg` and `-2.jpg` (the two positions of the flipping
  groups).
