# V9990: the line interrupt in the vertical blank

These are the hardware probe and the photos behind the line interrupt's
last line in `V9990::scheduleHscan()` (`src/video/v9990/V9990.cc`).

## Files

| File | What it is |
|------|------------|
| `hirqsnow.z80` | HIRQSNOW probe source. Its header describes every test. |
| `hirqsnow_N_powergraph.jpg` | Test N on a real V9990 (0: the table). |
| `hirqsnow_N_openmsx_before_fix.png` | Tests 0 and 6 in openMSX before the fix. |
| `hirqsnow_N_openmsx_after_fix.png` | Test N in openMSX after the fix. |

Test machine: a Hotbit (MSX1, 3.58 MHz Z80) with a Tecnobytes PowerGraph
(real V9990), NTSC, photographed on a Sony PVM monitor in black-and-white
mode, so the photos show brightness only.

The openMSX screenshots come from a Panasonic FS-A1GT with
`-ext video9000`, using `screenshot -raw`. openMSX doesn't emulate the
palette snow (stray dots from palette writes) yet, so its screenshots have
none.

Build the probe with sjasmplus (`sjasmplus hirqsnow.z80` writes
`HIRQSNOW.COM`). It is an MSX-DOS .COM program and is MSX1-safe. Keys 1-7
select a test; ESC returns to DOS.

## Screen

Bitmap mode B1 (256 × 212, 4 bpp, NTSC), R#16 = 0. Four vertical stripes:
colour 0 (black, the backdrop), 1 (black), 2 (grey) and 3 (white). The
test's number is shown as that many white blocks at the top left.

## Test 0: the table

For each IL (R#10-11), the lines from VR's rise to the line interrupt
(port 66h bit 1, R#9 IEH set). POL polls the flag with interrupts off.
INT takes it as an IM1 interrupt (0038h hooked for the frame; the MSX
VDP's interrupt is held off meanwhile). "---": none within 300 lines.

| IL | 0 | 100 | 211 | 212 | 213 | 226 | 227 | 228 | 229 | 230-235 |
|----|---|-----|-----|-----|-----|-----|-----|-----|-----|---------|
| Real V9990, POL and INT | 50 | 150 | 261 | 262 | 1 | 14 | 15 | 16 | 17 | --- |
| openMSX before the fix | 50 | 150 | 261 | 262 | 1 | 14 | 15 | 16 | 17 | 18-23 |
| openMSX after the fix | 50 | 150 | 261 | 262 | 1 | 14 | 15 | 16 | 17 | --- |

An earlier version of the probe tested IL 236, 246-248, 255, 261 and 262
on the same board: none came.

## Tests 1-7

Each writes a 147-byte palette burst (entries 15-63, none shown; about 18
lines on this Z80) from the line interrupt, every frame.

| Test | Line interrupt | Real V9990 |
|------|----------------|------------|
| 1 | none (no burst) | clean |
| 2 | IL 214, polled | snow in the bottom border |
| 3 | IL 226, polled | clean |
| 4 | IL 227, polled | clean |
| 5 | IL 229, polled | clean (the burst ends just before the top border) |
| 6 | IL 231, polled | "MISS": the interrupt never comes, no burst |
| 7 | IL 227, IM1 | clean, no MISS |

## What this shows

- IL counts from the picture's first line: VR rises at IL 212, and the
  frame is 262 lines.
- The line interrupt comes in the picture, in the bottom border and in the
  4 lines after it, up to IL 229 (17 lines after VR rises).
- It never comes from IL 230 on. That is where the manual's NTSC layout
  (section 13.3.13: 212 display, 14 border, then 4 erase, 3 sync and 15
  erase lines, 14 border) puts the vertical sync. Values past the frame's
  262 lines never come either.
- The line interrupt reaches the Z80 on this board: IM1 gives the same
  line as polling.

## Not known

- PAL timing, interlace, and other R#16 (display adjust) values.
- The per-line mode (R#11 IEHM). openMSX leaves it unchanged.
- Whether the line flag is ever set for IL 230 and up, or only the
  interrupt is missing. The polled column (the flag itself) shows nothing
  either.
