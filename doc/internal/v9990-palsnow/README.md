# V9990: palette snow

These are the hardware probes and the photos behind the palette snow.

## Files

| File | What it is |
|------|------------|
| `palsnow.z80` | PALSNOW probe source. Its header describes every test. |
| `palsnow2.z80` | PALSNOW2: PALSNOW plus tests A-I, test 7 in each colour mode. |
| `palsnow_N_powergraph.jpg` | Test N on a real V9990. Test 4 has two photos, `4a` and `4b`. |
| `palsnow_2_openmsx_before_fix.png` | Test 2 in openMSX before the fix: no snow. |
| `palsnow_N_openmsx_after_fix.png` | Test N in openMSX after the fix. |
| `palsnow2_K_powergraph.jpg` | PALSNOW2 test K (A-I) on a real V9990. |
| `palsnow2_K_openmsx_after_fix.png` | PALSNOW2 test K in openMSX after the fix. |
| `palread.z80` | PALREAD probe source: palette reads in place of writes. |
| `palread_N_powergraph.jpg` | PALREAD test N (0-8) on a real V9990. |
| `palread_2_openmsx_before_fix.png` | PALREAD test 2 in openMSX before reads snowed. |
| `palread_N_openmsx_after_fix.png` | PALREAD test N in openMSX after the fix. |

Test machine: a Hotbit (MSX1, 3.58 MHz Z80) with a Tecnobytes PowerGraph
(real V9990), NTSC, photographed on a Sony PVM monitor in black-and-white
mode, so the photos show brightness only. The colour casts in them come
from the camera: the white stripe also looks violet. A faint
horizontal line across the grey and white stripes, also in test 1, is not
snow.

The openMSX screenshots come from a Panasonic FS-A1GT with
`-ext video9000`, using `screenshot -raw`.

Build the probes with sjasmplus (`sjasmplus palsnow.z80` writes
`PALSNOW.COM`, `sjasmplus palsnow2.z80` writes `PALSNOW2.COM`,
`sjasmplus palread.z80` writes `PALREAD.COM`). They are MSX-DOS .COM
programs and are MSX1-safe. Keys 1-8 (and A-I in PALSNOW2, 0-8 in
PALREAD) select a test; ESC returns to DOS.

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

## PALSNOW2: tests A-I, test 7 in each colour mode

PALSNOW2 is PALSNOW with tests 1-8 unchanged, except that its blocks are
8 dots wide with 8-dot gaps. Each letter runs test 7's burst (entries
15-63 = white, all frame long, every other frame) in another B1 colour
mode, with the four stripes drawn in that mode's own pixel format. The
letter's index (A = 1, ..., I = 9) is shown as white blocks on lines
16-23. The modes' register bits come from the V9990 application manual,
section 9.1: R#6 CLRM (bits 1-0), R#13 PLTM (bits 7-6) and YAE (bit 5).

| Key | Mode | R#6 | R#13 | Stripe bytes (colours 0-3) | Picture dots read the palette? | Real V9990 |
|-----|------|-----|------|----------------------------|--------------------------------|------------|
| A | BP2 | 80h | 00h | 00h 55h AAh FFh | yes | dots everywhere, as test 7 |
| B | BP4 | 81h | 00h | 00h 11h 22h 33h | yes (test 7 itself) | dots everywhere, as test 7 |
| C | BP6 | 82h | 00h | 00h 01h 02h 03h | yes | dots everywhere, as test 7 |
| D | BD8 | 82h | 40h | 00h 00h 92h FFh | no | borders only; the 00h stripes are clean too |
| E | BD16 | 83h | 00h | 0000h 0000h 4210h 7FFFh | no | borders only |
| F | BYJK | 82h | 80h | 00h 00h 80h F8h (J = K = 0) | no | borders only |
| G | BYJKP | 82h | A0h | 00h (YJK), 18h (entry 1), 28h (entry 2), F0h (YJK) | stripes 1-2 yes, 0 and 3 no | borders and stripes 1-2; stripe 0 clean, starting exactly at stripe 1's left edge |
| H | BYUV | 82h | C0h | as F | no | borders only |
| I | BYUVP | 82h | E0h | as G | as G | borders and stripes 1-2, as G |

The border is the backdrop (entry 0) in every mode, so it goes through the
palette in all of them. G and I settle it: the same black on screen snows
in stripe 1 (a palette dot) and stays clean in stripe 0 (a YJK dot). So
the write disturbs the palette's output, not the output mixer.

## PALREAD: palette reads

PALREAD uses PALSNOW's screen and reads the palette data port where
PALSNOW writes it. Entries 4-63, which no pixel shows, hold R = n,
G = n + 11, B = n + 22, so a wrong read would show. "ERR" at the top
counts the reads that came back different from the palette. It stayed
00000 in every test.

| Test | What it does | Real V9990 |
|------|--------------|------------|
| 0 | table: lines from VR's rise to its fall; lines a 147-byte write burst and read burst take; entries 2, 3, 15 read in the blank | VR 50; write 18, read 17; P2 = 16 16 16, P3 = 31 31 31, P15 = 15 26 5 (all right) |
| 1 | no palette access | clean |
| 2 | entry 15 read, 3 bytes per line, from picture line 60 (every other line: the loop is longer than a line) | dots at a fixed x on the left of the picture |
| 3 | the same, entry 2 (a shown colour) | dots, as 2 |
| 4 | a 147-byte read burst at VR's rise | dots in the bottom border only |
| 5 | the same burst 16 lines after VR's rise | clean |
| 6 | read bursts for a whole frame, every other frame | dots in slanted bands over the picture, the grey stripe and the top border |
| 7 | entries 0-63 read for a whole frame, every other frame | dots in the picture, the grey stripe and both borders |
| 8 | PALSNOW test 7 (write bursts), the reference | dots everywhere |

So reads snow at the same places as writes, and still return the right
data. A read burst is not slower than a write burst, so the manual's "A
WAIT signal is output till the data has been prepared" (section 12.1,
p. 75) doesn't hold the CPU until a free slot: a read takes the palette
RAM from the picture, as a write does.

## What this shows

- Every byte written to or read from the palette data port (P#1, port
  61h) makes one dot (for reads, see below). Writing the palette pointer (R#14) makes none (inferred: tests 2
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
- Only dots whose colour comes out of the palette snow (PALSNOW2): every
  dot of the paletted modes, the border (the backdrop) in every mode, and
  the palette dots of BYJKP and BYUVP. The picture of BD8, BD16, BYJK and
  BYUV never snows, BD8's all-zero dots included.

## Not known

- The dot's exact width.
- Its colour when R, G and B differ: every test wrote equal R, G and B.
- The colour of a read's dot. The photos show read dots as bright as
  white write dots, but the monitor was in black-and-white mode, and a
  small bright dot blows out to full white whatever its colour.
- How many dots a read makes. Test 2's three reads per line show two
  columns of dots, some doubled, where PALSNOW's three writes show three.
  Maybe the chip prefetches the next byte after each read.
- Whether writes in the horizontal blank snow.
- PAL timing, the 512- and 640-wide modes and the P1 and P2 modes. Only
  B1 was tested.
- Sprites and cursors. They read the palette, so they should snow, also
  in the direct-colour modes.

## openMSX

The model: the palette RAM has a single access port and the CPU gets
priority. So during a CPU access every palette lookup made by the video
output returns the entry the CPU accesses. In openMSX a read or write of
the palette data port starts a short period (`SNOW_DURATION` in
`src/video/v9990/V9990PixelRenderer.cc`, 8 UC ticks: one B1 dot) in which
the rasterizer draws with a palette that has the accessed entry's colour
in all 64 entries. That way only the dots that read the palette snow: the
border, the paletted modes, the palette dots of BYJKP and BYUVP, sprites
and cursors.

The duration is a time, so the dots are wider (counted in dots of the
mode) in the modes with a higher horizontal resolution than B1. That is
not verified on hardware.

The snow is only drawn when the `accuracy` setting is `pixel`. The
`v9990_palette_snow` setting (on by default) turns it off.

The `*_openmsx_after_fix.png` screenshots were made with an earlier
implementation of the snow; it has been reworked since. PALSNOW2's G and I
screenshots also predate openMSX's BYJKP and BYUVP support: they show G as
F and I as H, without snow on stripes 1-2.
