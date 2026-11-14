# External video and genlock work

Target order: Philips NMS 8280, then Sony HB-F900 with HBI-F900 AV Creator.
The Sony HBI-V1 cartridge is separate from the HBI-F900 AV Creator.

## Current openMSX integration points

- `VDP::setExternalVideoSource()` accepts a `RawFrame`; Pioneer LaserDisc
  already feeds it through `PioneerLDControl::updateVideoSource()`.
- `VDP` enables the external source through the relevant MSX1 or MSX2
  superimpose register bits. `PixelRenderer` and `SDLRasterizer` pass the frame
  to the rasterizer.
- `Video9000` is another example of video composition, but combines two
  emulated outputs rather than a host capture device.
- NMS 8280 machine definitions, video control ports, a front panel and
  frame-based VDP digitization are present. The digitizer lives in `VDP`, not
  `VDPCmdEngine`.
- The HB-F900 machine definition includes its Video Utility ROM. The separate
  `Sony_HBI-F900` extension supplies the AV Creator's F7 control latch and
  shared external audio. The HBI-V1 cartridge has its own extension.

## Hardware behavior to establish before emulation

The NMS 8280 `SET VIDEO` firmware exposes normal, digitize, superimpose,
and external-only modes. The published example uses `SCREEN 8`, `SET VIDEO 1`,
and `COPY SCREEN` to digitize. Ports F6/F7 and the VDP external-video bits have
been investigated with the NMS 8280 firmware, but analog behavior and
digitizer pixel mapping still need comparison with real hardware. The
front-panel controls have a GUI representation. A displayed captured frame
alone does not prove that `COPY SCREEN` writes the correct bytes to VRAM;
that path needs its own test.

The original NMS 8280 disk-ROM dump SHA-1
`69f3dbfc1d516cd09a1a7e286049f99e37ca90d9` includes memory-mapped FDC
register values. After those bytes are normalized to `0xFF`, it matches the
NMS 8250 disk ROM SHA-1 `c3efedda7ab947a06d9345f7b8261076fa7ceeef`.
Use the normalized ROM in the machine definition.

Local ROM inventory checked against File-Hunter's 15 August 2026 openMSX set:
the NMS 8250 BIOS, sub-ROM, and normalized disk ROM satisfy the NMS 8280
definition, and all HB-F900 ROMs in its machine definition match. The alternate
`hb-f900_disk2.rom` was also retained locally without replacing the configured
disk ROM. File-Hunter's RomDB separately has two 32 KiB HBI-V1 dumps, SHA-1
`dbafa0f6ffa6dbfadbf631d209c5c9837ad0e100` and
`94c4089f91af3d5f2e8db97bb004fa13bbae0d20`. They differ in 260 bytes;
both are assigned to the experimental HBI-V1 extension. ROM binaries stay
outside Git.

## Current preview path

The `external_video_file` setting accepts a 640x480 P6 PPM file. The VDP
reloads one complete image at each frame boundary and uses the last valid
image if a writer is in the middle of replacing it. The path is available on
all machines for development. On a V9938, external superimpose is selected by
R#9 bits 5:4 = 01 with transparent background pixels (R#8 bit 5 clear).
This is frame-based composition. NMS 8280 digitization into VRAM and its front
panel have separate implementations. Analog genlock phase is not emulated.

Generate a still frame with:

```
python Contrib/external_video_bridge.py --output external-video.ppm --test-pattern
```

From the openMSX console, set `external_video_file` to its absolute path and
select superimpose in MSX2 BASIC with `SET VIDEO 2 : COLOR ,0,0`. A machine
without the NMS 8280 video hardware can be used to inspect the VDP path but
does not thereby become an NMS 8280. To feed live video once FFmpeg can open
the selected capture device, run the bridge with FFmpeg input arguments after
`--`, for example `-- -f dshow -i video=DEVICE_NAME` on Windows. FFmpeg must be
installed separately. The bridge publishes each complete frame atomically.
FFmpeg's official device documentation lists DirectShow (`dshow`) on Windows,
Video4Linux2 (`v4l2`) on Linux, and AVFoundation (`avfoundation`) on macOS:
https://www.ffmpeg.org/ffmpeg-devices.html

The preview was checked with a 640x480 test pattern and with FFmpeg's moving
`testsrc2` source on a Philips NMS 8255 using the new executable. Screenshots
showed the external frame behind MSX BASIC text and a changed external frame
two seconds later. This verifies frame-based visual composition, not USB
capture, NMS 8280 hardware behavior, digitization, or analog synchronization.

The HB-F900 and its HBI-F900 AV Creator are modeled separately. The base
computer alone is not the complete capture system. The Sony Video Utility ROM
uses port F7 and directly sets the VDP display mode for `CALL COMPUTER`,
`CALL SUPER` and `CALL VIDEO`; `CALL VIDEOIN(0..3)` selects input control bits.
See `sony-hbi-f900.md` for ROM offsets and tests. Do not infer HBI-F900
behavior from HBI-V1 documentation.

## Remaining work

1. Compare NMS 8280 and HBI-F900 pixel data, video levels, input routes and
   timing with physical hardware. The HBI-F900 `CALL BRIGHT` analog level and
   physical input switching are still unmodeled.
2. Test a real USB capture device, webcam, and network stream on each supported
   host platform. Field order and latency remain to be measured.
3. Make host video and audio deterministic in rewind and replays, or define
   an explicit policy for those features.
4. Verify normal and abnormal openMSX shutdown on Windows, Linux and macOS.
   Synthetic Windows parent-process tests passed; full application testing is
   still needed.
5. Keep commercial ROM binaries outside the repository and prepare focused
   review commits and reproducible tests for upstream.

## Sources

- NMS 8280 service manual:
  https://download.file-hunter.com/Manuals/Philips%20NMS%208280%20Service%20Manual.pdf
- NMS 8280 disk ROM dump identification:
  https://www.msx.org/pt-br/node/59225
- File-Hunter openMSX ROM set and HBI-V1 dumps:
  https://download.file-hunter.com/System%20ROMs/
  https://download.file-hunter.com/System%20ROMs/RomDB%20SystemROMs%20OpenMSX/
- MAME MSX2 machine slot map (older NMS 8280 disk dump marked bad):
  https://github.com/mamedev/mame/blob/master/src/mame/msx/msx2.cpp
- blueMSX description of NMS 8280 modes and digitization:
  https://www.msxblue.com/manual/digitization_c.htm
- blueMSX NMS 8280 digitizer implementation, useful as a comparison rather
  than hardware proof:
  https://github.com/libretro/blueMSX-libretro/blob/master/Src/Memory/romMapperNms8280VideoDa.c
- HB-F900 and HBI-F900 overview:
  https://www.msxblue.com/manual/hbf900_c.htm
- Sony HBI-F900 AV Creator and SonyVideo1 `CALL VIDEOIN`:
  https://www.msx.org/wiki/Sony_HBI-F900
  https://www.msx.org/wiki/CALL_VIDEOIN
