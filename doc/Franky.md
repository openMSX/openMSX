# Franky Emulation Information

This document describes the SuperSoniqs Franky expansion cartridge emulated
by openMSX and how to use it.

## Overview

The SuperSoniqs Franky (2009) is an MSX cartridge containing a Sega Master
System VDP (Sega 315-5124 "VDP1" / "SMS1" or 315-5246 "VDP2" / "SMS2") with
its integrated SN76489 PSG, plus the glue logic that maps them into the MSX
I/O space. It contains no ROM. The shipped extensions use the VDP2 (SMS2)
chip, as some games like Dizzy only work well with it.

* I/O ports: `0x48`/`0x49` (V-counter/H-counter reads, PSG writes) and
  `0x88`/`0x89` (VDP data/control).
* The Sega VDP interrupt output is connected to the cartridge slot's `/INT`
  line.
* The Sega VDP output is a **separate video source** (`Franky`), next to the
  MSX VDP output and, if present, the V9990/Video9000 outputs. It cannot be
  overlaid on the MSX VDP output; only one video source is displayed at a
  time.

A real Franky is built (or ordered) in one specific configuration. openMSX
ships one extension per video standard, both with the VDP2 chip:

| Extension     | Video standard | VDP chip             |
| ------------- | -------------- | -------------------- |
| `franky-ntsc` | NTSC (PAL-M)   | VDP2 (Sega 315-5246) |
| `franky-pal`  | PAL            | VDP2 (Sega 315-5246) |

Insert one of them with `ext franky-ntsc` or `ext franky-pal` on the console,
or through the extension list in the GUI. See *Hardware variants* below for
the VDP1 chip.

Only one Franky can be used at a time: the video standard and VDP chip are
fixed by the extension you insert. Inserting a second Franky is not refused,
but it has no usable video output (openMSX prints a warning). The two devices
also share the same I/O ports and would generate interrupts independently, so
software could never tell which VDP interrupted.

## Settings

The settings below are device-scoped: they exist while the Franky is
inserted and are saved with the other openMSX settings. In the GUI they are
listed in *Settings > Advanced > All settings*; on the console use
`set <setting> <value>`.

| Setting | Values (default) | Description |
| ------- | ---------------- | ----------- |
| `<device> auto video switch` | `on` (default), `off` | Automatically selects the video source (see below). `<device>` is the Franky device name, for example `Franky auto video switch`. |

Related settings:

* `videosource` (global setting, not remembered between sessions): selects
  which video source is displayed; see *Manual video source selection*.
* `Franky_SN76489_volume`: output volume of the Franky's PSG. The PSG
  registers under the sound device name `Franky_SN76489` (and so do its
  balance and per-channel mute settings). The standalone SN76489 extension
  uses the name `SN76489`, so the two are easy to tell apart.

## Automatic video source switching

The `<device> auto video switch` setting (e.g. `Franky auto video switch`)
is **on by default**. While it is on, openMSX selects the video source
automatically, once per Franky frame:

1. If the MSX VDP has an interrupt enabled (the vertical interrupt, and on
   V9938/V9958 also the line interrupt), the MSX output is selected, always.
2. Otherwise, if the Franky's Sega VDP has an interrupt enabled (vertical or
   line interrupt), the `Franky` output is selected.
3. Otherwise the current video source is left unchanged.

In other words: MSX VDP first, Franky second. While the MSX VDP is
generating interrupts the normal MSX output is shown; when software that
runs on the Franky takes over (it disables the MSX VDP interrupts and uses
the Sega VDP interrupts instead), the Franky output is shown. Only the MSX
VDP and the Franky are considered; the V9990 and Video9000 are ignored.

The check is done once per frame, so a switch can take up to one frame
(~16.7 ms NTSC / ~20 ms PAL). While the setting is on, changing
`videosource` manually has no lasting effect: it is overridden again on the
next frame.

## Manual video source selection

Turn the `<device> auto video switch` setting **off** to control the video
source manually:

* Console: `set videosource Franky` (or `set videosource MSX`; the possible
  values are the installed video sources).
* GUI: *Settings > Video > Misc > Video source to display*.

The `videosource` setting is not saved between openMSX sessions, so with the
auto switch off the source has to be selected again after each start.

## Hardware variants

The video standard and the VDP chip type are properties of the emulated
cartridge, so they are configured in the extension file, not as openMSX
settings. To use the VDP1 chip (Sega 315-5124), copy one of the shipped
extension files from `share/extensions` to the `extensions` folder of your
openMSX user directory (see the manual, chapter *Writing Hardware
Descriptions*), rename it (e.g. to `franky-ntsc1.xml`) and change the `<vdp>`
element:

    <vdp>VDP1</vdp>

Restart openMSX and insert the new extension. The same can be done for any
other combination of video standard and VDP chip, but only one Franky can be
used at a time.

Reasons to use a VDP1 configuration:

* Testing or creating patches that need to reproduce VDP1-specific behavior.
* Running Ys: The Vanished Omens, if it ever gets a Franky patch.

## Compatible software

The Franky works with software written or converted for it, for example:

* **SSMS** — runs Sega Master System games using the Franky's VDP and PSG.
* **Alexito Franky Conversions** — converted games targeting the Franky.
* **SG-1000 conversions** targeting the Franky can use the Franky PSG for
  more faithful audio.
* **VGMPlay** detects the Franky VDP as well, for more faithful audio.

## Debugging tools

openMSX's ImGui debugger adds three viewers for the Franky's Sega VDP, shown
in the *Debugger* menu while a Franky is inserted. They inspect the Sega VDP
(the MSX VDP has its own *VDP ...* viewers) and work for both the
TMS9918-compatible modes and the SMS-exclusive mode 4, following the current
screen mode automatically. If the running software uses the MSX VDP instead,
these viewers show an idle chip.

The viewers only change the emulation where noted (the register viewer and
the CRAM color blocks write to the chip, as does editing the debuggables
below); all other options are display-only. Window positions and options are remembered between
sessions.

### SMS VDP tile viewer

*Debugger > SMS VDP tile viewer* — shows the pattern table and the name table.

* **Settings**: the values read from the chip (screen mode, foreground and
  background color, pattern/color/name table base address, visible rows and
  the color-0 replacement). Tick *Manual override* — or the checkbox of a
  single item — to edit a value for the viewer only; useful to explore
  alternative layouts without changing the running program. Overriding only
  the screen mode reads the chip's table registers with that mode's rules.
  Foreground and background color are used in text mode only, so those
  overrides are available only then. Visible rows is the screen height: 24
  in modes 0-3, where overriding it to 32 shows the rest of the 1 kB
  name-table slot. In mode 4 the pattern and color tables have no base
  registers, the color-0 replacement does not exist and the name table
  already shows the whole scroll map, so those items are fixed. *Override
  color table* paints every pattern with one color (graphics I/II only, the
  modes with a color table; useful to see the pattern shapes when the color
  table uses the same foreground and background).
* **CRAM** (mode 4 only): the 32 color RAM entries. Click a color block to
  pick one of the 64 colors of the chip; CRAM is written immediately, so the
  displayed output changes.
* **Pattern Table**: the complete pattern table as the current mode lays it
  out (mode 4 shows all 512 tiles). In mode 4 the *Pattern palette* selector
  (0/1) chooses which CRAM palette is used to draw them. Hovering a cell
  shows the pattern number, its VRAM address (and, in graphics I/II, its
  color address) and a zoomed preview; right-click copies the pattern (and
  color) data to the clipboard. On SMS1 the R#3/R#4 tile masks can make bit
  planes 0-1 and 2-3 come from different tiles; then both addresses are
  shown.
* **Name Table**: the table as it is displayed: mode 4 shows the full table
  (28 rows, or 32 in the 224/240-line modes) with 16-bit entries (tile
  number, horizontal/vertical flip and palette); the TMS modes show 24
  rows, or 32 when that override is on. Hovering a cell shows the
  column/row, the tile number, the name-table entry address and the
  pattern/color addresses, plus a preview.
  *Name table overlay* draws the tile number over each cell: two hex digits
  (three in mode 4, where tile numbers are 9-bit).
* Options: *Zoom* (1x–8x), *grid* (with color) and *beam*: a marker at the
  current raster beam position in Sega VDP pixels. In mode 4 the position is
  mapped through the scroll registers (including the R#0 scroll locks), so
  it points at the tile that is currently displayed; it is most useful while
  the emulation is paused.

### SMS VDP sprite viewer

*Debugger > SMS VDP sprite viewer*

* **Settings**: as in the tile viewer: the values read from the chip (sprite
  type, size, magnification, color-0 transparency, pattern/attribute table
  base and visible lines) with optional manual overrides. The sprite type is
  `1` (TMS9918-style sprites) or `SMS` (mode 4); for SMS the sprite size is
  8x8 or 8x16 — the size bit doubles the height only. For SMS the pattern
  table is the sprite tile bank selected by R#6 bit 2: sprite tile numbers
  are 8-bit, so sprites use either the 256 tiles at 0x0000 or those at
  0x2000. Overriding it shows the other bank.
* **CRAM** (SMS only): editable as in the tile viewer.
* **Sprite patterns**: the raw sprite pattern table. For TMS: 256 8x8
  patterns or 64 16x16 sprites (monochrome; the color comes from the
  attributes). For SMS: the 256 tiles of the selected bank or 128 8x16
  sprites, in CRAM palette 1 and numbered as in the attribute table.
  Hovering shows the pattern number, its address and a zoomed preview;
  right-click copies the pattern data.
* **Sprite attributes**: all entries of the sprite attribute table (32 for
  TMS in an 8x4 grid, 64 for SMS in an 8x8 grid), each drawn with its
  pattern (and, for TMS, its color; color 0 sprites are transparent, as on
  the hardware). Hovering shows the sprite number, its address, the x/y and
  pattern (TMS: plus color and early clock; SMS: plus the entry address),
  the pattern actually used when the size, the R#6 tile bank or the SMS1
  tile mask changes it, and whether the hardware would display the sprite
  — and if not, why (display disabled, after the Y=0xD0 terminator,
  off-screen, a transparent TMS color 0, or beyond the per-line sprite
  limit). Right-click copies the entry.
* **Rendered sprites**: a simulation of the sprite layer alone — only the
  sprites are drawn (over the checkerboard), respecting the hardware limits:
  at most 8 sprites per line for SMS (4 for TMS), magnification, only the
  first sprites being zoomed and color-0 transparency. Hovering lists the
  sprites at that pixel; *Bounding box* draws the sprite rectangles (of the
  hovered sprites or of all), clipped to the screen: unlike on the MSX VDP,
  sprites do not wrap from the bottom to the top. *Max N sprites per line*
  toggles the per-line limit. *Show After Y=0xD0* (default off) also renders
  the entries after the terminator: the hardware stops reading the table at
  the first entry with Y=0xD0, always in the TMS modes and in 192-line mode
  for SMS (otherwise the option is disabled). The viewer follows the visible
  line count from Settings, including a manual override.
* Options (in *Settings*): *Zoom* (1x–8x), applied to all sections; *grid*
  (with color); and the *Checkerboard* colors and size, used as the background
  for transparent pixels everywhere (patterns and rendered sprites). Each
  checkerboard color has a *Reset to default* button in its color picker;
  picking the same color twice gives a solid background.

### SMS VDP register viewer

*Debugger > SMS VDP register viewer* — shows the registers R#0–R#10 and the
status register S#0, grouped in sections (mode registers, table base
registers, scroll registers, color register, line counter and status
register). Each register is shown with its individual bit names:

* Click a bit button to toggle it, or type a value in the hex field. Writes
  go to the chip immediately, exactly like the program writing the register.
* The explanation area (toggle it in the context menu) spells out what the
  current register values select: the screen mode (an invalid combination of
  mode bits is named as such), whether the display is enabled, the VINT/HINT
  enables, sprite size/magnification/shift, the scroll locks, the colors, the
  resulting table base addresses and the status flags (VINT flag, sprite
  overflow and collision, fifth sprite number). In graphics II the color and
  pattern tables are shown with their address masks. On the SMS1, the mode 4
  masks (R#2 bit 0, R#3, R#4 bits 2–0, R#5 bit 0, R#6 bits 1–0) are shown
  whenever they are active, because they garble the picture. The status flags
  are shown as a CPU read would return them now. For R#10 both the reload
  value and the live line counter are shown (the chip raises a line interrupt
  every N+1 lines).
* Hovering a bit or an explanation item highlights the bits and the
  explanation that belong together; clicking an explanation item that
  depends on a single bit toggles that bit.

### Debuggables

The Sega VDP state is also available as debuggables, which can be opened in
the generic hex editor via *Debugger > Add hex editor* (view, search, export;
edit a byte to write it):

| Name | Size | Notes |
| ---- | ---- | ----- |
| `SMS VDP regs` | 16 | R#0–R#15 (only R#0–R#10 are functional) |
| `SMS VDP status regs` | 1 | status register S#0 as a read would return it (no side effects), read-only |
| `SMS VDP VRAM` | 16 kB | video RAM |
| `SMS VDP CRAM` | 32 | color RAM (6-bit colors) |

The same names can be used on the console or in scripts, for example
`debug read "SMS VDP VRAM" 0x3f00` or `debug write "SMS VDP CRAM" 0 0x3f`.
The debuggables belong to the Sega VDP chip; with a second Franky inserted
(which is not useful) they get a ` (1)` suffix.
