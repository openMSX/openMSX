# Philips NMS 8280 video hardware: findings and what is emulated

This answers the open points of `external-video-genlock.md` (ports F6/F7, the
VDP external-video bits, the front panel). Sources: the NMS 8280 service manual
(block diagram), the user manual (chapters 8 to 11), the MSX2 sub-ROM of the
real machine (disassembled with openMSX itself), the Philips VIDEO GRAPHICS and
demo disks, and measurements: every `SET VIDEO` parameter was run on its own
and the VDP registers and the port shadows (`FAF7`, `FFF8`) were read back.
Nothing here was checked on real hardware yet.

## Ports

| Port | Block (service manual) | What is known |
|---|---|---|
| 0xF6 | "mode select 1", data D2-D6 | outputs DIZ (digitize level), YMIX, B/W. **Bit 5 = 0 selects the video mix** (`OUT &HF6,&B11011111`). The sub-ROM never uses this port. |
| 0xF7 | "mode select 2", data D3-D7, input Vdet | written by `SET VIDEO`, see below. The sub-ROM reads it at boot (`IN F7 AND 77h <> 77h`) to detect the hardware, after writing `0xD3`. |
| 0xF8 | audio mix | written by `SET VIDEO` (shadow in `FFF8`); `F8[1:0] = (audio >> 2) & 3`. **Meaning unknown.** |

Bits that are not connected read as 1; the device returns the latch otherwise.

## `SET VIDEO mode,Ym,CB,sync,audio,input,AV`

| Parameter | Writes | Emulated as |
|---|---|---|
| mode 0 | R#9 S1:S0 = 00, F7[7:6] = 11 | only the computer |
| mode 1 | S = 01, R#8 TP = 1, F7[7:6] = 10 | digitize mode: opaque computer image |
| mode 2 | S = 01, TP = 0, F7[7:6] = 10 | superimpose (color 0 transparent) |
| mode 3 | S = 10, F7[7:6] = 01 | only the external video |
| Ym | F7 bit 5 | external video at half intensity |
| CB | R#8 bit 4 | with MS = 0 in a bitmap mode: only the external video |
| sync | F7 bit 3 | state only |
| audio | F7[1:0] = ~(v & 3), F8[1:0] = v >> 2 | routing of the external audio (see below) |
| input | F7 bit 2 | state only |
| AV | F7 bit 4 | state only |

Bits 7:6 of F7 are the inverse of S1:S0 (they drive the mode indicators). The
audio parameter is the **fifth** one; the fourth only accepts 0 or 1.
Parameters 4, 6 and 7 select analog routes (sync source, SCART or other input,
SCART output) that have no effect with one digital source; the latches can be
read with `debug read {Video control ports} N` (0 = F6, 1 = F7, 2 = F8, 3 = the
front panel indicators).

Video mix (bit 5 of F6 = 0), following the table of chapter 10 of the manual:

| `SET VIDEO` | Result |
|---|---|
| 0 | the computer image at 50% |
| 1 | both complete images mixed 50/50 |
| 2 | computer image at 50% over the full video (transparent areas show the video) |
| 2,1 | same, with the video halved |

## Digitizing

`COPY SCREEN` works through the VDP, not through the ports: while bit 6 of R#0
(DG) is set, the external video frame is stored in the displayed VRAM page
(SCREEN 5 to 8). Assumptions not verified on hardware: R#7 acts as a mask on the
digitized values (that is why the manual uses `COLOR ,,255`), SCREEN 5-7 take the
closest palette color, SCREEN 8 is plain GRB 3-3-2. The digitization level slider
scales what is digitized.

## Front panel

A window with the three indicators (AV IN/OUT, AV IN, SUPERIMPOSE), the
audio/video source selector and the sliders for the digitization level (setting
`external_video_level`), the audio mix (`external_audio_mix`) and the video mix
(`video_mix_level`, with the click-stop at the bottom: only the external video).
The superimpose indicator follows F7[7:6]; the source indicators only work when
superimposing or with the video mix slider at EXT, as in the manual.

## Audio of the external source

`external_video_bridge.py --audio-output FILE` captures the audio with a second
FFmpeg process and writes a ring buffer file (layout in the docstring of the
script); the `ExternalAudio` device plays it. It resamples the 48 kHz input to the
host rate with a cubic interpolation and keeps the wanted delay
(`external_audio_delay`) by changing the reading speed by at most 1%, so the drift
between the capture clock and the emulation causes no clicks. A stopped or
restarted capture gives silence, never old audio. `external_audio_mix` is the
INT/EXT slider (the sound of the computer is scaled through
`MSXMixer::setInternalGain()`). With `external_audio_follow_set_video` on (off by
default) the audio routing bits of F7 are obeyed: bit 0 = 0 mixes the external
audio in the right channel, bit 1 = 0 in the left one; after a reset no external
audio is mixed in until `SET VIDEO` selects it.

## Sony HBI-V1

Memory mapped cartridge: 32 kB ROM at 0x4000-0xBFFF (`AB` header, `CALL`
statement handler at 0x4035), a 256x212 byte digitizer VRAM read through a window
at 0x7E00 (auto-increment) and the registers at 0x7FFC-0x7FFF (command, start
block, mode and block sizes, delay). Modes: YJK, YJK with 4 bit Y, RGB 3-3-2,
clear. The ROM copies the image with 212 `OTIR` of 256 bytes. **The register
behavior and the block tables follow the mapper of blueMSX-plus (GPL) as a
reference; they are not verified against the real cartridge.** Tested: `CALL AD` in
SCREEN 8 (MSX2) and SCREEN 12 (MSX2+), and the digitizing program of the Sony disk.

## ROM sets

The "plain" NMS 8280 set is identical to the NMS 8250 ROMs. The German (G) and
French (F) sets are dumps of the actual chips: the BIOS differs in the key and
language tables, the disk ROM of G is the 8250 v1.08, and the sub-ROM differs from
the 8250 one in 114 bytes of code (the use of F7 and F8 is the same). G and F only
differ in one sub-ROM byte, the default of register 12 of block 2 of the CMOS (2
international, 4 F, 5 G), which nothing reads.

## Not done

Rewind and replays (digitizing and audio depend on the host video), real capture
cards and network streams, F8, the video mix together with mode 3, the power and
floppy indicators of the front panel, the MSVC project files.

## Testing tips

In direct mode BASIC goes back to the text screen at the prompt; to look at a
SCREEN 5 or 8 image keep a program running, for instance
`SCREEN 8:SET VIDEO 0,0,1:A$=INPUT$(1)`. Settings and data in `~/.openMSX/share`
(machine XML files, shaders) take precedence over the installed ones.
