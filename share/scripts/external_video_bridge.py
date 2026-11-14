#!/usr/bin/env python3
"""Feed openMSX's external_video_file setting from FFmpeg or a test pattern.

FFmpeg emits raw RGB24 frames at 640x480. Each complete frame is published as
an atomic P6 PPM replacement so openMSX never sees a partially written image.

Optionally (--audio-output) the audio of the source is captured by a second
FFmpeg process and written to a ring buffer file read by openMSX's
"external audio" device (setting external_audio_file). Ring file layout, all
little-endian:

    0   4  magic "MXA1"
    4   4  sample rate (Hz)
    8   2  channels
    10  2  format (1 = signed 16 bit)
    12  4  capacity in frames
    16  8  total number of frames written so far (updated after the data)
    32  .  capacity * channels * 2 bytes of circular sample data
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import threading
import time

WIDTH = 640
HEIGHT = 480
FRAME_BYTES = WIDTH * HEIGHT * 3
HEADER = b"P6\n640 480\n255\n"

AUDIO_RATE = 48000
AUDIO_CHANNELS = 2
AUDIO_CAPACITY = 96000  # frames: two seconds
AUDIO_FRAME_BYTES = AUDIO_CHANNELS * 2
AUDIO_HEADER_SIZE = 32
AUDIO_CHUNK = 4096  # bytes read from FFmpeg at once (about 21 ms)


def capture_backend() -> str:
    if sys.platform == "win32":
        return "dshow"
    if sys.platform == "darwin":
        return "avfoundation"
    if sys.platform.startswith("linux"):
        return "v4l2"
    raise ValueError(f"automatic device selection is unavailable on {sys.platform}")


def device_arguments(device: str) -> list[str]:
    backend = capture_backend()
    if backend == "dshow":
        return ["-f", backend, "-i", f"video={device}"]
    if backend == "avfoundation":
        return ["-f", backend, "-i", f"{device}:none"]
    return ["-f", backend, "-i", device]


def resolve_ffmpeg(requested: str | None) -> str:
    if requested:
        return requested
    if found := shutil.which("ffmpeg"):
        return found
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        raise FileNotFoundError("FFmpeg is not installed") from None


def list_devices(ffmpeg: str, output: Path | None = None) -> int:
    backend = capture_backend()
    if backend == "v4l2":
        devices = [str(path) for path in sorted(Path("/dev").glob("video[0-9]*"))]
        if output:
            output.write_text("\n".join(devices), encoding="utf-8")
        else:
            print("Video4Linux2 devices:", *devices, sep="\n")
        return 0
    if backend == "dshow":
        command = [ffmpeg, "-hide_banner", "-list_devices", "true",
                   "-f", backend, "-i", "dummy"]
    else:
        command = [ffmpeg, "-hide_banner", "-f", backend,
                   "-list_devices", "true", "-i", ""]
    # FFmpeg normally exits with an error after listing devices.
    if output:
        result = subprocess.run(command, capture_output=True, text=True, errors="replace")
        listing = result.stderr + result.stdout
        if backend == "dshow":
            devices = re.findall(r'"([^"]+)" \(video\)', listing)
        else:
            video_listing = listing.split("AVFoundation audio devices:", 1)[0]
            devices = [name for _, name in re.findall(
                r'\[(\d+)\] ([^\r\n]+)', video_listing)]
        output.write_text("\n".join(devices), encoding="utf-8")
    else:
        subprocess.run(command, check=False)
    return 0


def audio_device_arguments(device: str) -> list[str]:
    backend = capture_backend()
    if backend == "dshow":
        return ["-f", backend, "-i", f"audio={device}"]
    if backend == "avfoundation":
        return ["-f", backend, "-i", f"none:{device}"]
    return ["-f", "pulse", "-i", device or "default"]


def list_audio_devices(ffmpeg: str, output: Path | None = None) -> int:
    backend = capture_backend()
    # One device per entry: "name" or "name<TAB>friendly description". The name
    # is what --audio-device expects.
    devices: list[str] = []
    if backend == "v4l2":
        try:
            result = subprocess.run(["pactl", "list", "sources"],
                                    capture_output=True, text=True, errors="replace")
            name = description = active = None
            ports: dict[str, str] = {}

            def flush() -> None:
                if name and not name.endswith(".monitor"):
                    # "Internal Microphone - Built-in Audio Analog Stereo"
                    label = " - ".join(x for x in (ports.get(active or ""), description) if x)
                    devices.append(f"{name}\t{label}" if label else name)

            in_ports = False
            for line in result.stdout.splitlines():
                stripped = line.strip()
                if stripped.startswith("Source #"):
                    flush()
                    name = description = active = None
                    ports = {}
                    in_ports = False
                elif stripped.startswith("Name:"):
                    name = stripped[len("Name:"):].strip()
                elif stripped.startswith("Description:"):
                    description = stripped[len("Description:"):].strip()
                elif stripped.startswith("Active Port:"):
                    active = stripped[len("Active Port:"):].strip()
                    in_ports = False
                elif stripped == "Ports:":
                    in_ports = True
                elif in_ports and ": " in stripped and "(type:" in stripped:
                    port, rest = stripped.split(": ", 1)
                    ports[port] = rest.split(" (type:")[0].strip()
                elif in_ports and not stripped:
                    in_ports = False
            flush()
        except FileNotFoundError:
            devices = []
    else:
        if backend == "dshow":
            command = [ffmpeg, "-hide_banner", "-list_devices", "true",
                       "-f", backend, "-i", "dummy"]
        else:
            command = [ffmpeg, "-hide_banner", "-f", backend,
                       "-list_devices", "true", "-i", ""]
        result = subprocess.run(command, capture_output=True, text=True, errors="replace")
        listing = result.stderr + result.stdout
        if backend == "dshow":
            devices = re.findall(r'"([^"]+)" \(audio\)', listing)
        elif "AVFoundation audio devices:" in listing:
            audio_listing = listing.split("AVFoundation audio devices:", 1)[1]
            devices = [name for _, name in re.findall(r'\[(\d+)\] ([^\r\n]+)', audio_listing)]
    if output:
        output.write_text("\n".join(devices), encoding="utf-8")
    else:
        print("Audio devices:", *(d.replace("\t", "   ") for d in devices), sep="\n")
    return 0


class AudioRing:
    """Writer of the audio ring buffer file (see the module docstring)."""

    def __init__(self, path: Path) -> None:
        self.frames = 0
        self.file = path.open("w+b")
        header = (b"MXA1" + struct.pack("<IHHI", AUDIO_RATE, AUDIO_CHANNELS, 1, AUDIO_CAPACITY)
                  + struct.pack("<Q", 0))
        self.file.write(header.ljust(AUDIO_HEADER_SIZE, b"\0"))
        self.file.write(bytes(AUDIO_CAPACITY * AUDIO_FRAME_BYTES))
        self.file.flush()

    def write(self, data: bytes) -> None:
        data = data[:len(data) - len(data) % AUDIO_FRAME_BYTES]
        view = memoryview(data)
        while view:
            slot = self.frames % AUDIO_CAPACITY
            count = min(len(view) // AUDIO_FRAME_BYTES, AUDIO_CAPACITY - slot)
            self.file.seek(AUDIO_HEADER_SIZE + slot * AUDIO_FRAME_BYTES)
            self.file.write(view[:count * AUDIO_FRAME_BYTES])
            view = view[count * AUDIO_FRAME_BYTES:]
            self.frames += count
        # publish the new position only after the data is in place
        self.file.seek(16)
        self.file.write(struct.pack("<Q", self.frames))
        self.file.flush()

    def close(self) -> None:
        self.file.close()


def run_audio(command: list[str], ring: AudioRing, stop: threading.Event) -> None:
    """Pump the PCM output of an FFmpeg process into the ring buffer."""
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    assert process.stdout is not None
    try:
        while not stop.is_set():
            data = process.stdout.read(AUDIO_CHUNK)
            if not data:
                break
            ring.write(data)
    finally:
        if process.poll() is None:
            process.terminate()
        process.stdout.close()
        process.wait()


def publish(path: Path, rgb: bytes) -> None:
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("wb") as output:
        output.write(HEADER)
        output.write(rgb)
    for attempt in range(5):
        try:
            os.replace(temporary, path)
            return
        except PermissionError:
            if attempt == 4:
                raise
            time.sleep(0.01)


def test_pattern(path: Path) -> None:
    rgb = bytearray(FRAME_BYTES)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            i = (y * WIDTH + x) * 3
            rgb[i] = x * 255 // (WIDTH - 1)
            rgb[i + 1] = y * 255 // (HEIGHT - 1)
            rgb[i + 2] = 255 if (x // 40 + y // 40) % 2 else 0
    publish(path, rgb)


def read_frame(stream) -> bytes | None:
    frame = bytearray(FRAME_BYTES)
    view = memoryview(frame)
    offset = 0
    while offset < FRAME_BYTES:
        chunk = stream.readinto(view[offset:])
        if not chunk:
            return None
        offset += chunk
    return frame


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        help="PPM path used by openMSX's external_video_file setting")
    parser.add_argument("--ffmpeg", help="FFmpeg executable (default: PATH or imageio-ffmpeg)")
    parser.add_argument("--test-pattern", action="store_true",
                        help="write one test frame and exit")
    parser.add_argument("--list-devices", action="store_true",
                        help="list local video capture devices and exit")
    parser.add_argument("--list-devices-file", type=Path,
                        help="write video device names to a UTF-8 file and exit")
    parser.add_argument("--stop-file", type=Path,
                        help="stop capture when this file is created")
    parser.add_argument("--device", help="capture device name (Windows), index (macOS), or path (Linux)")
    parser.add_argument("--audio-output", type=Path,
                        help="ring buffer file used by openMSX's external_audio_file setting; "
                             "enables audio capture")
    parser.add_argument("--audio-device",
                        help="audio capture device (PulseAudio source on Linux, DirectShow name on "
                             "Windows, AVFoundation device on macOS). Without it, the audio of the "
                             "FFmpeg input arguments after -- is used")
    parser.add_argument("--audio-gain", type=float, default=0.0,
                        help="gain in dB applied to the captured audio (microphones are often quiet)")
    parser.add_argument("--audio-test-tone", action="store_true",
                        help="write a 440 Hz test tone to --audio-output instead of capturing audio")
    parser.add_argument("--list-audio-devices", action="store_true",
                        help="list local audio capture devices and exit")
    parser.add_argument("--list-audio-devices-file", type=Path,
                        help="write audio device names to a UTF-8 file and exit")
    parser.add_argument("ffmpeg_input", nargs=argparse.REMAINDER,
                        help="FFmpeg input arguments after --")
    args = parser.parse_args()
    if args.list_devices or args.list_devices_file:
        return list_devices(resolve_ffmpeg(args.ffmpeg), args.list_devices_file)
    if args.list_audio_devices or args.list_audio_devices_file:
        return list_audio_devices(resolve_ffmpeg(args.ffmpeg), args.list_audio_devices_file)
    if args.output is None:
        parser.error("--output is required for capture or --test-pattern")
    input_args = args.ffmpeg_input
    if input_args and input_args[0] == "--":
        input_args = input_args[1:]
    if args.device and input_args:
        parser.error("use either --device or FFmpeg input arguments after --")
    if args.device:
        try:
            input_args = device_arguments(args.device)
        except ValueError as error:
            parser.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.test_pattern:
        test_pattern(args.output)
        return 0

    if not input_args:
        parser.error("supply --device, FFmpeg input arguments after --, or --test-pattern")

    command = [resolve_ffmpeg(args.ffmpeg), "-hide_banner", "-loglevel", "warning", *input_args,
               "-an", "-vf", f"scale={WIDTH}:{HEIGHT}",
               "-pix_fmt", "rgb24", "-f", "rawvideo", "-"]
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    assert process.stdout is not None

    stop_audio = threading.Event()
    audio_ring: AudioRing | None = None
    audio_thread: threading.Thread | None = None
    if args.audio_output is not None:
        ffmpeg = resolve_ffmpeg(args.ffmpeg)
        if args.audio_test_tone:
            audio_input = ["-re", "-f", "lavfi", "-i", f"sine=frequency=440:sample_rate={AUDIO_RATE}"]
        elif args.audio_device is not None:
            audio_input = audio_device_arguments(args.audio_device)
        else:
            audio_input = input_args  # audio of the same input as the video
        audio_filter = ["-af", f"volume={args.audio_gain}dB"] if args.audio_gain else []
        audio_command = [ffmpeg, "-hide_banner", "-loglevel", "warning", *audio_input,
                         "-vn", *audio_filter, "-ac", str(AUDIO_CHANNELS), "-ar", str(AUDIO_RATE),
                         "-f", "s16le", "-"]
        args.audio_output.parent.mkdir(parents=True, exist_ok=True)
        audio_ring = AudioRing(args.audio_output)
        audio_thread = threading.Thread(
            target=run_audio, args=(audio_command, audio_ring, stop_audio), daemon=True)
        audio_thread.start()

    if args.stop_file:
        def watch_stop() -> None:
            while process.poll() is None:
                if args.stop_file.exists():
                    stop_audio.set()
                    process.terminate()
                    return
                time.sleep(0.1)
        threading.Thread(target=watch_stop, daemon=True).start()
    try:
        while (frame := read_frame(process.stdout)) is not None:
            publish(args.output, frame)
    except KeyboardInterrupt:
        return 0
    finally:
        stop_audio.set()
        if process.poll() is None:
            process.terminate()
        process.stdout.close()
        process.wait()
        if audio_thread is not None:
            audio_thread.join(timeout=2)
        if audio_ring is not None:
            audio_ring.close()
    return process.returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FileNotFoundError:
        print("Video capture failed: FFmpeg executable not found. "
              "Install FFmpeg or pass --ffmpeg PATH.", file=sys.stderr)
        sys.exit(1)
