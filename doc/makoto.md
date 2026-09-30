# SuperSoniqs Makoto

Insert the **Makoto** extension to emulate the YM2608 OPNA cartridge. It provides
six FM voices, three SSG voices, six internal rhythm sounds and ADPCM-B playback
with 256 KiB sample RAM. No additional rhythm-ROM file is required.

Use the normal **Makoto volume** control for overall volume. The
`makoto_psg_volume` setting adjusts SSG balance, corresponding to the board's
separate SSG control. The audio channel viewer supports all 16 voices, including
individual muting and recording.

The debugger exposes **Makoto registers** and **Makoto ADPCM RAM**. Pause playback
before editing sample memory. Normal openMSX save states and rewind are supported.

Software should use ports 14h-17h. Reading status at 16h avoids the real-hardware
conflict with the Music Module at 14h. This emulation uses the cartridge's fixed
8 MHz clock and direct timer-interrupt connection.

Makoto remains optional: it consumes host CPU even when silent. Analogue
amplifier distortion, headphone-load response and the physical pot taper are not
calibrated. Implementation and regression details are in
[the developer notes](internal/makoto.md).
