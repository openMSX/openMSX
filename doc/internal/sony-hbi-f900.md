# Sony HB-F900 and HBI-F900 AV Creator

The HBI-F900 is the external AV Creator attached to the HB-F900 video interface.
It is distinct from the HBI-V1 cartridge. The HB-F900's 16 KiB Video Utility
ROM (SHA-1 `558b7383544542cf7333700ff90c3efbf93ba2a3`) contains SonyVideo1
BASIC. The ROM is supplied by the user and is not included here.

## Firmware evidence

Disassembly addresses below refer to the ROM mapped at 0x4000. At 0x405E the
startup routine reads port 0xF7, masks bit 7 and returns if it is high. Thus the
Video Utility ROM leaves its video extension inactive without the external
unit. The `Sony_HBI-F900` extension provides an F7 device whose detection bit
is low. The base HB-F900 machine retains the open-bus value 0xFF at that port.

The ROM's `CALL VIDEOIN` handler at 0x43B2 writes bits 3:2 of F7. With the
extension attached, `CALL VIDEOIN(0..3)` was traced writing 0xD0, 0xD4, 0xD8
and 0xDC. `CALL AUDIO` at 0x437E writes the inverted two-bit channel mask to
F7 bits 1:0. `CALL BRIGHT` at 0x4438 changes bit 4; `CALL GENLOCK ON/OFF` at
0x4406/0x4416 changes bit 3. The firmware also programs VDP registers directly:
in a headless ROM test, `CALL COMPUTER`, `CALL SUPER` and `CALL VIDEO` resulted
in R#9 bits 5:4 equal to 00, 01 and 10 respectively. Existing VDP external
frame composition handles these display modes.

## Current implementation and test

Insert the extension with `-ext Sony_HBI-F900` while running the
`Sony_HB-F900` machine. It adds the F7 latch and the shared external audio
device. Sony's `CALL AUDIO` controls the audio channel routing. The existing
host video source and VDP frame path serve the external video. The Sony ROM
remains responsible for BASIC commands and VDP register changes.

With the user's HB-F900 ROMs, `-testconfig` succeeded and the machine booted
headlessly. Without the extension, F7 read 0xFF and `CALL SUPER` did not change
R#9. With the extension, F7 detection worked and `CALL SUPER`/`CALL VIDEO`
changed R#9 to 0x10/0x20. In SCREEN 8, `CALL DIGITIZE ON:COPY SCREEN` with a
synthetic external frame changed 24,150 bytes of a 54,272-byte VRAM sample
compared with a run without capture. This verifies that Sony firmware reaches
the shared digitizer path; it is not a pixel-accurate hardware comparison.

## Remaining hardware questions

- F7 source selection is latched, but the GUI exposes one external video
  source. The physical HBI-F900's input switching and analog signal routes
  need a manual, schematic or hardware trace before they can be modeled.
- `CALL BRIGHT` and analog genlock phase are not emulated. The relationship
  between F7's brightness bit and the analog video level is not established.
- Audio routing is implemented, but capture from a real HBI-F900 or equivalent
  line source has not been tested. The video and audio are frame/sample based.
- Captured host input is not yet deterministic in rewind or replay.
- HBI-V1 is a separate cartridge and does not stand in for HBI-F900.

Sources: [HB-F900](https://www.msx.org/wiki/Sony_HB-F900),
[HBI-F900](https://www.msx.org/wiki/Sony_HBI-F900),
[SonyVideo1 BASIC](https://www.msx.org/wiki/SonyVideo1_BASIC), and the user's
HB-F900 Video Utility ROM identified above.
