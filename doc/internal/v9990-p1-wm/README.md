# V9990 P1: command writes reach the other layer unless WM blocks them

These are the hardware probes and photos behind the P1 `dataLatch` model in
`src/video/v9990/V9990CmdEngine.cc` (see `p1Write()`).

## Files

| File | What it is |
|------|------------|
| `p1cmd.z80` | P1CMD probe source. Every command runs with WM = FFFFh. |
| `p1cmd2.z80` | P1CMD2: the same probe, but WM follows the destination (R#46 = FFh, R#47 = 00h for layer A; 00h, FFh for layer B). |
| `p1cmd_powergraph.jpg` | P1CMD on a real V9990. |
| `p1cmd2_powergraph.jpg` | P1CMD2 on a real V9990. |
| `p1cmd_openmsx_before_fix.png` | P1CMD in openMSX before the fix. The leak bars are missing. |
| `p1cmd_openmsx_after_fix.png` | P1CMD in openMSX after the fix. It matches `p1cmd_powergraph.jpg` band for band. |
| `p1cmd2_openmsx_after_fix.png` | P1CMD2 in openMSX after the fix. It matches `p1cmd2_powergraph.jpg`. |

The openMSX screenshots come from C-BIOS_MSX1 with `-ext gfx9000`, using
`set videosource GFX9000` and `screenshot -raw -doublesize`.

Build the probes with sjasmplus (`sjasmplus p1cmd.z80` writes `P1CMD.COM`).
Both are MSX-DOS .COM programs and are MSX1-safe. Press any key to return to
DOS.

Test machine: a Hotbit (MSX1) with a Tecnobytes PowerGraph (real V9990),
no V7040.

## Screen layout

- Left half: layer A (VRAM0), x 0-127. Right half: layer B (VRAM1), x 0-127.
- The palette is a grey ramp (pixel value v gives brightness 2v), so the
  photos only need to show whether a pixel is on or off.
- Lines 0-7 hold a reference ramp written by the CPU to both layers
  (pixel = x & 15).
- Every test command is 64x8 with LOP = WC=SC.

| Test | Command | R#6 = 05h, lines | R#6 = 01h, lines |
|------|---------|------------------|------------------|
| T1 | LMMM A(0,0) -> A(0,y) | 16 | 128 |
| T2 | LMMM A(0,0) -> A(1,y), odd DX | 32 | 144 |
| T3 | LMMM A(0,0) -> B(512,y) | 48 | 160 |
| T4 | LMMV A, FC = 9999h | 64 | 176 |
| T5 | LMMV B, FC = CCCCh | 80 | 192 |
| T6 | LMMM B(512,0) -> A(0,y) | 96 | 208 |

If a command's CE bit never clears, the probe draws a white 8x8 block at
x = 120 in layer A.

## Results on hardware

The target layer was correct in every test. This table shows what appeared in
the layer the command did *not* target:

| Test | P1CMD, R#6 = 05h | P1CMD, R#6 = 01h | P1CMD2 |
|------|------------------|------------------|--------|
| T1, T2: A -> A | empty | solid | empty |
| T3: A -> B, leak into A | striped | striped | empty |
| T4: fill A, leak into B | striped | striped | empty |
| T5: fill B, leak into A | solid | solid | empty |
| T6: B -> A, leak into B | solid | solid | empty |

## Model

- The command engine has a 16-bit data output register. Its low byte drives
  VRAM0, its high byte drives VRAM1, and both chips get the same address.
- A P1 write loads only the target chip's byte:
  - LMMV loads the FC byte of the target layer;
  - LMMM loads the source pixel shifted into its nibble, with the other
    nibble 0.
- The other byte keeps what the last command write to that chip left there.
  CPU writes do not change the register, and reset clears it.
- Both chips are written through WM & the nibble mask of the target pixel.

This explains every leak in the table:

- **Striped or solid.** A stale byte from LMMM (`0F`/`F0`) gives a striped
  bar. A stale FC byte (`99`/`CC`) gives a solid bar.
- **A -> A leaks only in run 2.** This has nothing to do with R#6. In run 1
  the VRAM1 byte is still 0 after reset. Run 1's T5 leaves `CC` there, and run
  2's T1 and T2 write that byte into layer B.

This matches the application manual: "In P1 mode, writing is prohibited on the
side not specified as the transfer destination (Layer A: R#46, Layer B:
R#47)". The chip doesn't protect the other layer by itself. The program has to
set WM so that it is protected.

## Not covered by these probes

- Exact pixel values in a leak: the photos only show on/off.
- Other logical operations, TP, DIX/DIY.
- Commands other than LMMM and LMMV.
