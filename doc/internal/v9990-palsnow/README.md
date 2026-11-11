# V9990: palette snow

These are the hardware probe and the photos behind the palette snow.

## Files

| File | What it is |
|------|------------|
| `palsnow.z80` | PALSNOW probe source. Its header describes every test. |
| `palsnow_N_powergraph.jpg` | Test N on a real V9990. Test 4 has two photos, `4a` and `4b`. |
| `palsnow_2_openmsx_before_fix.png` | Test 2 in openMSX before the fix: no snow. |
| `palsnow_N_openmsx_after_fix.png` | Test N in openMSX after the fix. |

Test machine: a Hotbit (MSX1, 3.58 MHz Z80) with a Tecnobytes PowerGraph
(real V9990), NTSC, photographed on a Sony CRT. The colour casts in the
photos come from the camera: the white stripe also looks violet. A faint
horizontal line across the grey and white stripes, also in test 1, is not
snow.

The openMSX screenshots come from a Panasonic FS-A1GT with
`-ext video9000`, using `screenshot -raw`.

Build the probe with sjasmplus (`sjasmplus palsnow.z80` writes
`PALSNOW.COM`). It is an MSX-DOS .COM program and is MSX1-safe. Keys 1-8
select a test; ESC returns to DOS.

## Screen

- Bitmap mode B1 (R#6 = 81h, 256 × 212, 4 bpp, NTSC). The backdrop is
  colour 0.
- Four vertical stripes, 64 dots each: colour 0 (black, the backdrop),
  colour 1 (also black), colour 2 (grey, 16,16,16) and colour 3 (white,
  31,31,31).
- The test's number is shown as that many white blocks at the top left.
- Every palette write either writes an entry no pixel uses, or rewrites an
  entry with the value it already has. So anything that appears on screen
  is snow.
- Each frame starts at the rise of the status VR bit. Lines are counted by
  the status HR bit.

## Tests and results on hardware

| Test | What it writes | Real V9990 |
|------|----------------|------------|
| 1 | nothing | clean |
| 2 | entry 15 (unused) = white, one entry (3 bytes) per line, from picture line 60, early in the line | dots on those lines, three per line, in the left part of the picture, in slanted rows |
| 3 | entry 2 (the grey stripe's) = its own grey, the same way | dimmer dots at the same places |
| 4 | entry 15 = white, the 3 bytes sent just after HR rises | dots, denser, in the left part of the next line |
| 5 | entries 15-63 = white: a 147-byte burst at VR's rise | dots in the bottom border only, across its whole width |
| 6 | the same burst 16 lines later | clean |
| 7 | that burst repeated from VR's rise to the next VR rise | dots everywhere: the picture, every stripe, the top border |
| 8 | entries 0-15 rewritten with their own values (48 bytes) at picture line 100 | a band of dots across the full width at about line 100 |

## What this shows

- Every byte written to the palette data port (P#1, port 61h) makes one
  dot. Writing the palette pointer (R#14) makes none (inferred: tests 2
  and 3 write it once per line and show three dots per line, not four).
- It doesn't matter which entry is written, whether any pixel uses it, or
  whether the value changes.
- The dot appears where the beam is at the moment of the write: in the
  picture over any colour (the backdrop's too), and in the top and bottom
  borders.
- Writes in the invisible part of the vertical blank (16-34 lines after VR
  rises) leave nothing.
- The dot is about one line tall and one to two dots wide.
- Its colour is the colour being written: white writes give bright dots,
  grey writes dim ones, black writes show as dark dots on grey and white.

## Not known

- The dot's exact width.
- Its colour when R, G and B differ: every test wrote equal R, G and B.
- Whether palette reads also snow.
- Whether writes in the horizontal blank snow.
- PAL timing, the 512- and 640-wide modes and the P1 and P2 modes. Only
  B1 was tested.
