# Franky Emulation Information

This document describes the SuperSoniqs Franky expansion cartridge emulated
by openMSX (`franky`) and how to use it.

## Overview

The SuperSoniqs Franky (2009) is an MSX cartridge containing a Sega Master
System VDP (Sega 315-5124 "SMS1" or 315-5246 "SMS2") with its integrated
SN76489 PSG, plus the glue logic that maps them into the MSX I/O space. It
contains no ROM. SMS2 is the default choice as some games like Dizzy only
work well with it.

* I/O ports: `0x48`/`0x49` (V-counter/H-counter reads, PSG writes) and
  `0x88`/`0x89` (VDP data/control).
* The Sega VDP interrupt output is connected to the cartridge slot's `/INT`
  line.
* The Sega VDP output is a **separate video source** (`Franky`), next to the
  MSX VDP output and, if present, the V9990/Video9000 outputs. It cannot be
  overlaid on the MSX VDP output; only one video source is displayed at a
  time.

Insert the extension with `ext franky` on the console, or through the
extension list in the GUI.

## Settings

The settings below are device-scoped: they exist while the Franky is
inserted and are saved with the other openMSX settings. In the GUI they are
listed in *Settings > Advanced > All settings* (alphabetically, under their
`franky_...` names); on the console use `set <setting> <value>`.

| Setting                   | Values (default)       | Description |
| ------------------------- | ---------------------- | ----------- |
| `franky_video_standard`   | `NTSC` (default), `PAL` | Video standard of the Sega VDP. `NTSC` also covers the PAL-M timing. It selects both the pixel clock and the frame geometry (262 lines / ~59.9 Hz for NTSC, 313 lines / ~49.7 Hz for PAL), so a wrong value gives wrong game speed. |
| `franky_vdp_variant`      | `SMS2` (default), `SMS1` | Sega VDP chip variant: `SMS1` = 315-5124, `SMS2` = 315-5246. It changes small hardware details (palette, timing quirks). Changing it recreates the emulated chip. |
| `franky_auto_video_switch` | `on` (default), `off` | Automatically selects the video source (see below). |

Related settings:

* `videosource` (global setting, not remembered between sessions): selects
  which video source is displayed; see *Manual video source selection*.
* `SN76489_volume`: output volume of the Franky's PSG. The PSG uses the same
  device name as the standalone SN76489 extension, so both share this setting
  name.

## Automatic video source switching

`franky_auto_video_switch` is **on by default**. While it is on, openMSX
selects the video source automatically, once per Franky frame:

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

Turn `franky_auto_video_switch` **off** to control the video source
manually:

* Console: `set videosource Franky` (or `set videosource MSX`; the possible
  values are the installed video sources).
* GUI: *Settings > Video > Misc > Video source to display*.

The `videosource` setting is not saved between openMSX sessions, so with the
auto switch off the source has to be selected again after each start.

## Compatible software

The Franky works with software written or converted for it, for example:

* **SSMS** — runs Sega Master System games using the Franky's VDP and PSG.
* **Alexito Franky Conversions** — converted games targeting the Franky.
* **SG-1000 conversions** targeting the Franky can use the Franky PSG for
  more faithful audio.
* **VGMPlay** detects the Franky VDP as well, for more faithful audio.