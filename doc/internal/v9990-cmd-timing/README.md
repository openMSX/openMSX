# V9990: command timing

These are the hardware probe and the measurements behind the command
timing in `src/video/v9990/V9990CmdEngine.cc` (the `Tm` namespace,
`grantAccess()` and `accessStep()`).

## Files

| File | What it is |
|------|------------|
| `99timing.z80` | 99TIMING probe source (version 2). Its header describes the method and the file format. |
| `99timing_powergraph.txt` | Its output on a real V9990. |
| `99timing_openmsx_before.txt` | Its output in openMSX before this change. |
| `99timing_openmsx_after.txt` | Its output in openMSX after this change. |

Test machine: a Hotbit (MSX1, 3.58 MHz Z80) with a Tecnobytes PowerGraph
(real V9990, 21.48 MHz crystal) and a GR8NET, NTSC, R#16 = 0.

The openMSX files come from a Sharp HB-8000 with
`-ext Panasonic_FS-FD1A -ext gr8net-timer -ext video9000`, under MSX-DOS. The
output (718 kB) doesn't fit that machine's 720 kB disk, so the last
configuration (B6) of each openMSX file comes from a second run of a
build with only the last three modes (`sjasmplus -DTAIL`).

Build the probe with sjasmplus (`sjasmplus 99timing.z80` writes
`99TIMING.COM`). It is an MSX-DOS .COM program, takes about 25 minutes
and writes `99TIMING.TXT`. It finds the GR8NET's counter by putting each
slot in page 1 and checking that it counts Z80 clocks (the `G` line is
the slot found); it doesn't use the GR8NET's I/O ports.

## Method

The clock is the GR8NET's 24-bit counter at 3.579545 MHz, so the unit is
one Z80 clock (a scanline is 227.5). Each run:

1. waits in HALT for the V9990's line interrupt, so the command starts at
   a known raster position;
2. waits a chosen number of clocks more;
3. reads the counter and writes the opcode to R#52;
4. waits in HALT for the command-end interrupt and reads the counter
   again.

A calibration line (CAL) gives the fixed overhead. Samples are rounded up
to 5 clocks (a HALT cycle). Repeats agree within 5 clocks.

34 configurations: P1; P2; B1, B3, B7, B0, B2, B4, B5 and B6 at 2, 4, 8
and 16 bits a pixel.

- **P1:** 224 cases in 7 environments: display on with sprites on or off,
  each started in the picture (line 100) and in the retrace (line 216);
  display off with sprites on or off; and a start at line 2, where the
  sprites are. Six small commands are also started at every clock of a
  scanline (three of them) or every 5 clocks.
- **The others:** 31 cases in 3 environments (display on with the cursor
  on or off, display off), and two start sweeps at 4 bits a pixel.
- Four LINEs are drawn on cleared rows and the rows read back (the V
  lines).

## What the hardware showed

### Words and accesses

The command engine works on 16-bit words: one byte from each of the two
VRAM chips. **Every word a command touches, source or destination, costs
2 accesses.**

| Command | Words touched |
|---------|---------------|
| LMMV | the destination word |
| LMMM | the source word and the destination word |
| CMMM | the destination word, plus a font word for each 16 pixels |
| BMXL, BMLX | the rectangle's word, plus a word for each 2 linear bytes |
| BMLL | a source and a destination word for each 2 linear bytes |
| LINE | the destination word, for each pixel |
| SRCH | the source word |
| PSET | the destination word |

Pixels in a word: 1 at 16 bits a pixel, 2 at 8, 4 at 4 (and in P2), 8 at
2. In P1 a word is the same byte position in layers A and B: 2 pixels of
the layer drawn. A row that doesn't start at a word's first pixel touches
one more word.

### When an access can happen

| Where | Accesses |
|-------|----------|
| Display off, or outside the picture's lines | any time |
| P1, P2: picture, the active part of the line | 48 slots, evenly spread |
| P1, P2: picture, horizontal blank, sprites on | one word (2 accesses) 21 clocks after the active part |
| P1, P2: picture, horizontal blank, sprites off | any time for the first 44.5 clocks |
| Bitmap modes: picture | any time, but for a stretch at the end of the horizontal blank: 21.6 clocks with the cursor on, 3.4 with it off (28.5 and 4.3 in B0, B2, B4) |

- In P1 and P2 that is 25 words a scanline with sprites on. P2 has the
  same slots as P1; its words hold twice the pixels.
- The accesses are rationed for 213 lines: the picture's 212 and the
  line before it. The borders and the retrace are free.
- A command changes speed as the raster moves between those regions.
- Starting on the lines the sprites are on changes nothing.
- With the display off, the sprite bit changes nothing.

### The engine's own speed

With nothing in its way the engine needs 0.69 clocks from one access to
its next, plus time that depends on the command and on the pixels in a
word. Clocks for each word, display off:

| Pixels in a word | 1 (16 bpp) | 2 (8 bpp) | 4 (4 bpp) | 8 (2 bpp) | P1 | P2 |
|---|---|---|---|---|---|---|
| LMMV | 1.38 | 1.38 | 2.06 | 3.09 | 1.41 | 2.10 |
| LMMM (2 words) | 2.86 | 2.88 | 2.87 | 3.55 | 2.98 | 2.98 |
| BMLL (2 words, 2 bytes) | 2.86 | 2.86 | 2.86 | 2.86 | 2.98 | 2.98 |

- BMXL and BMLX cost what LMMM does, except in P1 (1.22 and 1.41 clocks a pixel there).
- LINE: 2.70 clocks a pixel in every mode. SRCH: 2.7 clocks a word.
- LMMV and LMMM need 0.4 to 2.5 clocks more for each row.
- **In B0, B2 and B4 every engine time is about 1.55 times longer.** They
  run from the 14.3 MHz clock, not the 21.5 MHz crystal.
- B5 and B6 are as fast as B1 with the display off.

### Other findings

- **LINE wraps.** In P1 a line that passes the layer's edge goes on from
  the other edge of the same row, and takes its full length's time.
- **BMLL: a count with a low byte of 0 takes the time of 256 bytes more**
  (256, 1024, 2048, 4096 bytes: all of them, in every mode).
- BMLL takes 0.6 clocks a byte more when the source and destination
  bytes are in different chips (addresses of different parity). It needs
  no more accesses.
- LMMM takes more time when source and destination are at different
  places in their words (in P1: 0.5 clocks a pixel).
- The logical operation, transparency, the write mask, the layer, the
  vertical direction and the source's contents don't change the time.
- SRCH's time follows the distance to the pixel it finds.
- ADVN and an idle STOP raise CE at once. POINT never raises it by
  itself: it waits for its data to be read.
- In a linear address bit 0 selects the chip and the rest is the word's
  offset.
- B5 and B6 (31 kHz) work: both interrupts arrive. Their blocked stretch
  comes twice as often.

## openMSX against the hardware

Relative error of the median for the cases of 200 clocks or more:

| Modes | Before: median (90th percentile) | After: median (90th percentile) |
|-------|----------------------------------|---------------------------------|
| P1 (1061 cases) | 9.5% (97%) | 0.4% (2.8%) |
| P2 | 16.8% (103%) | 0.3% (1.6%) |
| B1, B3, B7 | 2.3-10.5% (32-307%) | 0.6-2.4% (1.5-5.7%) |
| B0, B2, B4 | 2.3-10.3% (51-235%) | 1.5-2.5% (3.0-9.1%) |
| B5, B6 (run as B7) | 6.9-14.1% (30-287%) | 2.9-6.5% (9-15%) |

- The start sweeps differ by 2 to 5 clocks on average after the change
  (8 to 16 before).
- The LINE readback: all 16 rows equal after the change (11 before).

## Not matched, not known

- **B5 and B6.** openMSX has no such modes: it runs them as B7, so their
  twice-a-line blocked stretch is missing.
- **BMXL and BMLX from an odd X in P1** are 12-14% too fast with the
  display off. The access count is right; some time for each row is
  missing.
- **2 bits a pixel:** LMMV in the overscan modes is 5-7% too slow.
- **BMLL's 256 bytes more:** only their time is emulated. Whether the
  chip also copies them was not measured.
- **LINE's wrap** was only measured in P1. In the other modes openMSX
  still ends the line at the image's edge.
- LMMC, LMCM, CMMC and CMMK (the CPU sets their pace) keep their old
  behaviour.
- PAL, interlace, other R#16 values and the 25 MHz crystal were not
  measured.
- The 5-clock resolution can't place single slots. The 48 slots are
  spread evenly here; the chip may group them.
