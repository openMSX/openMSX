"""Makoto's fixed built-in percussion and mid-voice save tests."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import re
import tempfile
import time
import wave
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("makotest", ROOT / "Contrib/makoto-test.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--openmsx", type=Path, required=True)
    parser.add_argument("--firmware-dir", type=Path, required=True)
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix="makoto-rhythm-", dir=ROOT / "derived"))
    source = (ROOT / "src/sound/YM2608AdpcmRom.hh").read_text()
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    source = source[source.index("YM2608_ADPCM_ROM"):]
    source = source[source.index("{") + 1:source.index("}")]
    data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", source))
    assert len(data) == 8192
    assert hashlib.sha256(data).hexdigest() == "53afd0fa9c62eda3e2be939e23f3adf48a2af8ad37bb1640261726c5d5adeba8"
    rom = out / "test.rom"
    rom.write_bytes(m.image(0x38))
    results = {"sample_sha256": hashlib.sha256(data).hexdigest(), "voices": {}}
    e = m.Emulator(args.openmsx, out, args.firmware_dir, rom, "ASCII16")

    def write(reg, value):
        e.command(f"debug write ioports 20 {reg}; debug write ioports 21 {value}")

    def step(seconds):
        e.command(f"after time {seconds} {{set pause on}}; set pause off")
        until = time.monotonic() + 20
        while e.command("set pause") != "true":
            assert time.monotonic() < until

    def record(label, channel, pan=0xc0):
        write(0x10, 0xbf)
        step(.04)
        write(0x18 + channel, pan | 0x1f)
        path = out / (label + ".wav")
        e.command("soundlog start " + m.tcl_path(path))
        write(0x10, 1 << channel)
        step(.32)
        e.command("soundlog stop")
        with wave.open(str(path)) as wav:
            assert wav.getsampwidth() == 2 and wav.getnchannels() == 2
            samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(-1, 2)
        samples = samples.astype(float)
        assert np.max(abs(samples)) < 32767
        return samples

    def peak(samples):
        return np.max(abs(samples), axis=0).tolist()

    def save(path):
        e.command("store_machine [machine] " + m.tcl_path(path))

    def restore(path):
        e.command("set old [machine]; set new [restore_machine " + m.tcl_path(path) +
                  "]; delete_machine $old; activate_machine $new; set pause on")

    def state(path):
        root = ET.fromstring(gzip.decompress(path.read_bytes()))
        return ET.tostring(m.sound_node(root))

    try:
        e.command("set pause on; ext Makoto; set mute off; set volume 50; set Makoto_volume 20")
        rhythm_channel = 8 if e.command("info exists {Makoto SSG_volume}") == "1" else 11
        write(0x11, 0x3f)
        for channel, name in enumerate(("bass", "snare", "cymbal", "hihat", "tom", "rim")):
            samples = record(name, channel)
            peaks = peak(samples)
            assert min(peaks) > 20, (name, peaks)
            assert np.array_equal(samples[:, 0], samples[:, 1]), name
            results["voices"][name] = {"peak": peaks}
            e.command(f"set Makoto_ch{rhythm_channel + channel}_mute true")
            muted = record(name + "-muted", channel)
            assert max(peak(muted)) <= 1, (name, peak(muted))
            e.command(f"set Makoto_ch{rhythm_channel + channel}_mute false")
        left = peak(record("left", 1, 0x80))
        right = peak(record("right", 1, 0x40))
        assert left[0] > 20 and left[1] <= 1, left
        assert right[1] > 20 and right[0] <= 1, right
        # Cymbal lasts long enough to capture and resume part-way through.
        write(0x1a, 0xdf)
        write(0x10, 4)
        step(.04)
        saved = out / "mid-cymbal.oms"
        save(saved)
        step(.06)
        first = out / "first-continuation.oms"
        save(first)
        restore(saved)
        step(.06)
        second = out / "restored-continuation.oms"
        save(second)
        assert state(first) == state(second), "Percussion continuation changed chip state"
        write(0x10, 0xbf)
        step(.05)
        silent = out / "stopped.wav"
        e.command("soundlog start " + m.tcl_path(silent))
        step(.1)
        e.command("soundlog stop")
        with wave.open(str(silent)) as wav:
            samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2")
        assert np.max(abs(samples.astype(float))) <= 1
        results["controls"] = "All six channel mutes, left/right pan and key-off passed"
        results["save"] = "Mid-cymbal restoration gives identical full chip state after 60 ms"
    finally:
        e.close()
    (out / "results.json").write_text(json.dumps(results, indent=2))
    print(out)
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
