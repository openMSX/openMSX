# V9990: the command registers after a command

This is the hardware probe and its output behind the end-of-command register
values in `src/video/v9990/V9990CmdEngine.cc` (see `endDecNY()`, `endRect()`,
`endBMLL()`, `endSRCH()` and `advancePointer()`).

## Files

| File | What it is |
|------|------------|
| `vdpend.z80` | VDPEND probe source. Its header describes the tests, the witnesses and the output format. |
| `vdpend_powergraph.txt` | `VDPEND.TXT` written by the probe on a real V9990. |

Test machine: a Hotbit (MSX1) with a Tecnobytes PowerGraph (real V9990), no
V7040, running MSX-DOS. The probe is an MSX-DOS .COM program and is
MSX1-safe. It takes about 9 minutes and writes `VDPEND.TXT` on the current
drive.

## How the probe works

R#32–R#52 are write-only, so the probe measures each end value by its effect:

- It runs in P1 with R#6 = 05h.
- Before each test, it refills VRAM. Every row y of layer A gets two marker
  pixels, so a SRCH along a row tells which row it is.
- Each test writes R#32–R#51, then R#52, and waits for CE.
- A "witness" command then measures one register. It writes every
  parameter it uses except the one being measured, so its result shows
  that register's end value.
- Each witness reruns the test from the same VRAM. 51 tests × 21 witnesses
  cover all 16 opcodes.
- Two calibration tests (`CAL`, `CAL2`) issue no command. They check that
  every witness reads back the values as written.

## Running it

Build the probe with sjasmplus (`sjasmplus vdpend.z80` writes `VDPEND.COM`)
and run it under MSX-DOS. To check openMSX, run it under MSX-DOS on an
emulated MSX with `-ext gfx9000` (for example an FS-A1GT with MSX-DOS 2) and
compare its `VDPEND.TXT` with `vdpend_powergraph.txt`. The files should match line for line, except the
status port's raster bits (only TR, BD and CE matter) and W0's reads of
R#32–R#52, which are write-only and return open bus.

Before the fix, 907 witness lines differed. After it, the two files match.

## Results on hardware

End values (P1 has 2048 rows, so SY and DY are seen modulo 2048):

| Command (R#52) | SX | SY | DX | DY | NY |
|---|---|---|---|---|---|
| LMMM, BMLL | kept | +NY | kept | +NY | FFFh |
| LMMV | 0 | +NY | kept | +NY | FFFh |
| LMMC, CMMC | 0 | kept | kept | +NY | FFFh |
| LMCM | kept | +NY | 0 | kept | FFFh |
| BMLX | kept | +NY | kept | kept | FFFh |
| BMXL, CMMM | kept | kept | kept | +NY | FFFh |
| CMMK (no kanji ROM) | kept | kept | kept | +NY | FFFh |
| LINE | 0 | kept | kept | the line's end | kept |
| SRCH | the end X | kept | 0 | kept | −1 |
| POINT | kept | +1 | 0 | kept | −2 |
| PSET, ADVN | 0 | kept | kept | +1 with AYE, −1 with AYE+AYM | −1 |
| STOP (no command running) | kept | kept | kept | kept | −1 |

- +NY means −NY when DIY is set.
- NX, ARG, LOP, WM, BC and FC bits 7–0 never change. NY 0 also ends at FFFh.
- SRCH ends SX at the found X, at 256 when nothing is found moving right,
  or at 2047 when nothing is found moving left.
- R#53–R#54 (BX) read SX after every command, and also with no command.
- Status BD is set by a SRCH that finds its colour, and stays set through
  later commands until FC (R#48–R#49) is written.
- A P1 LMMV into layer B draws with FC bits 7–0.
- LMCM with NX 0 moves 4096 pixels per row.
- An LMCM with an odd pixel count leaves the source's other pixel in the
  unused nibble of the last byte.
- LINE with MJ 3, MI 1 takes its first Y step after its first pixel.
