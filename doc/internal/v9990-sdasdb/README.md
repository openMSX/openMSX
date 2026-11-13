# V9990: R#22 SDA/SDB (layer disables)

These are the hardware probe and the photos behind the SDA/SDB layer
disables in `src/video/v9990/V9990PxConverter.cc` (see
`V9990P1Converter::convertLine()`).

## Files

| File | What it is |
|------|------------|
| `sdasdb.z80` | SDASDB probe source. Its header describes every phase. |
| `sdasdb_N_powergraph.jpg` | Phase N on a real V9990. |
| `sdasdb_4_openmsx_before_fix.png` | Phase 4 in openMSX before the fix: no sprite. |
| `sdasdb_N_openmsx_after_fix.png` | Phase N in openMSX after the fix. |

Test machine: a Hotbit (MSX1, 3.58 MHz Z80) with a Tecnobytes PowerGraph
(real V9990), NTSC, photographed on a Sony PVM monitor in black-and-white
mode, so the photos show brightness only.

The openMSX screenshots come from a Panasonic FS-A1GT with
`-ext video9000`, using `screenshot -raw`.

Build the probe with sjasmplus (`sjasmplus sdasdb.z80` writes
`SDASDB.COM`). It is an MSX-DOS .COM program and is MSX1-safe. Keys 1-5
select a phase; ESC returns to DOS.

## Screen

P1 mode (R#6 = 05h), R#13 = 0 (both layers use palette entries 0-15):

- Layer A: tile 1 (colour 1, red) on odd tile rows, giving horizontal bars.
- Layer B: tile 2 (colour 2, green) on odd tile columns, giving vertical
  bars.
- One sprite: pattern 8, solid colour 3 (white), 16 × 16, drawn on lines
  99-114, in front of the layers.
- Backdrop: colour 0 (dark blue). R#27 = 0 (layer A in front everywhere).

## Phases and results on hardware

| Phase | R#22 | Real V9990 |
|-------|------|------------|
| 1 | 00h | red bars over green bars, the sprite on top |
| 2 | 80h (SDA) | green bars only; no sprite |
| 3 | 40h (SDB) | red bars only; no sprite |
| 4 | C0h (SDA + SDB) | the sprite alone on the backdrop |
| 5 | 80h at VR's rise, 00h at picture line 106 | top as 2, from line 106 on as 1; only the sprite's lines from 106 on show |

Phase 5 waits for the line flag (port 66h bit 1, R#10 = 106) with the line
interrupt disabled (R#9 = 0). If the flag never came, the backdrop would
turn white. It didn't, so the flag is set even with the interrupt
disabled. openMSX already does that.

## What this shows

- R#22 bit 7 (SDA) hides layer A and bit 6 (SDB) hides layer B. A hidden
  layer is transparent: the other layer, or the backdrop, shows through.
- Either bit alone also hides the sprites. Both bits together show the
  sprites again, alone on the backdrop.
- A write mid-frame takes effect from the line it is written on, for the
  layers and the sprites.
- Bit 0 (SCBY8, layer B's Y-scroll bit 8) keeps its documented meaning.

## Not known

- The exact line and dot where a mid-frame write takes effect.
- Whether SCBY8 (bit 0) also changes mid-frame.
- P2, the bitmap modes and their cursors. Only P1 was tested.
- Sprites behind layer A (attribute P bit) under SDA/SDB.
- Why sprites return when both bits are set. The two bits may form a
  2-bit display-select field instead of two independent switches.

## openMSX

All five phases match the photos. Phase 5's 00h write lands on picture
line 106, and the split, sprite included, falls on that line.
