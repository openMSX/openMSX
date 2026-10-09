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
