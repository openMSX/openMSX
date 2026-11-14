# External video preview

This branch can put a live external video frame behind transparent MSX pixels.
It is a frame-based preview of superimpose. Digitization into VRAM, external
audio, external-only mode, analog line/phase synchronization, and the NMS 8280
front-panel controls are still pending.

## Quick test

Run from the repository root:

```powershell
python Contrib/external_video_bridge.py --output external-video.ppm --test-pattern
```

Start the newly built openMSX, then enter this in its console (F10), using the
absolute path to the generated file:

```tcl
set external_video_file C:/path/to/external-video.ppm
```

On an MSX2 machine with the optional video functions, select superimpose with
`SET VIDEO 2 : COLOR ,0,0` in BASIC. For a development preview on another
V9938 machine, R#9 bits 5:4 must equal `01`, R#8 bit 5 must be clear, and
background color index 0 must be used. The NMS 8280 machine definition uses
the normalized NMS 8250 disk ROM (`nms8250_disk.rom`, SHA-1
`c3efedda7ab947a06d9345f7b8261076fa7ceeef`) in the normal openMSX ROM
search path. ROM binaries are not included here.

## Live capture

In the built openMSX, open **Settings > Video > External video input...**.
The panel lists capture devices when opened; choose a device such as `USB Video`
or select **Network stream** and enter an RTSP/HTTP URL. Use **Start** to begin,
**Switch source** to change inputs, and **Stop** to disconnect. On Windows the
panel uses the `py -3` launcher by default. If Python or FFmpeg cannot be found,
set their executable paths under **Capture tools**. The URL is not saved in the
GUI preferences. The MSX software must still activate superimpose.
For local capture with **Capture audio** enabled, select an audio input as
well. FFmpeg's video-device selection alone does not select a microphone.

When openMSX starts the bridge, it passes its process ID. The bridge stops
video and audio capture when **Stop** is selected, openMSX exits normally, or
the openMSX process disappears unexpectedly. Both FFmpeg processes are
terminated before the audio ring file is closed.

The command-line bridge remains available for scripts and custom FFmpeg input
options.

Install an FFmpeg build with the operating system's capture backend. The bridge
scales decoded frames to 640x480 RGB and publishes only complete frames. List
available inputs, then select a webcam or USB analog capture device:

```powershell
python Contrib/external_video_bridge.py --list-devices
python Contrib/external_video_bridge.py --output external-video.ppm --device 'DEVICE NAME'
```

On Windows, `--device` takes the DirectShow video device name. On Linux it takes
a Video4Linux2 path such as `/dev/video0`; `--list-devices` prints the available
`/dev/video*` paths. On macOS it takes an AVFoundation index such as `0` (or a
device name); `--list-devices` prints the available inputs. Capture drivers and
FFmpeg must expose the device; appearing in OBS alone does not guarantee it
appears in this backend.

For unusual devices, pass native FFmpeg input options after `--`, for example
`-- -f dshow -video_size 720x576 -i 'video=DEVICE NAME'` on Windows.
If FFmpeg is not in `PATH`, supply `--ffmpeg /path/to/ffmpeg` before `--`.
Device formats and PAL/NTSC settings must be chosen for the actual capture
device. See [FFmpeg input device documentation](https://www.ffmpeg.org/ffmpeg-devices.html).

The tested path used FFmpeg's moving `testsrc2` source and a Philips NMS 8255
with its local ROMs. Two screenshots showed different external frames behind
the MSX BASIC text. No USB capture device or NMS 8280 ROM was used in that
test.
