# SuperSoniqs Makoto

Insert the **Makoto** extension to emulate the YM2608 OPNA cartridge. It provides
six FM voices, three SSG voices, six internal rhythm sounds and ADPCM-B playback
with 256 KiB sample RAM. No additional rhythm-ROM file is required.

Use the **Makoto** volume for FM, rhythm and ADPCM, and **Makoto SSG** for
SSG. Both use standard mixer volume sliders. All 16 voices support individual
muting and recording (13 in Makoto, 3 in SSG).

The debugger exposes **Makoto registers** and **Makoto ADPCM RAM**. Pause playback
before editing sample memory. Normal openMSX save states and rewind are supported.

Software should use ports 14h-17h. Reading status at 16h avoids the real-hardware
conflict with the Music Module at 14h. This emulation uses the cartridge's fixed
8 MHz clock and direct timer-interrupt connection.

Makoto remains optional: it consumes host CPU even when silent. Analogue
amplifier distortion, headphone-load response and the physical pot taper are not
modeled. Implementation notes are in
[the developer notes](internal/makoto.md).

## Multiple cartridges

Device, setting, IRQ and debugger names follow the extension's XML device ID.
A second Makoto is named `Makoto (1)`, for example
`{Makoto (1) SSG_volume}`, `{Makoto (1) registers}` and `{Makoto (1).IRQ}`.
Each cartridge retains its own RAM, timers and state. Real Makoto uses 14h-17h;
a copied configuration can map another instance to a different aligned range.
