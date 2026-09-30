"""Compare builds from identical restored music states, excluding resampler warm-up."""
import argparse
import importlib.util
import json
import tempfile
import time
import wave
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("makotest", ROOT / "Contrib/makoto-test.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
p = argparse.ArgumentParser(__doc__)
p.add_argument("--baseline", type=Path, required=True)
p.add_argument("--candidate", type=Path, required=True)
p.add_argument("--firmware-dir", type=Path, required=True)
p.add_argument("--rom", type=Path, required=True)
p.add_argument("--manifest", type=Path, required=True)
a = p.parse_args()
out = Path(tempfile.mkdtemp(prefix="makoto-audio-state-", dir=ROOT / "derived"))
songs = json.loads(a.manifest.read_text())["songs"]
report = []

def step(e, seconds):
    e.command(f"after time {seconds} {{set pause on}}; set pause off")
    deadline = time.monotonic() + 60
    while e.command("set pause") != "true":
        assert time.monotonic() < deadline

def configure(e):
    e.command("set pause on; set mute off; set volume 50; set Makoto_volume 20; set {Makoto SSG_volume} 20; set [lindex [info vars ?akoto_psg_volume] 0] 50")

for track in (0, 3, 9, 13):
    run = out / f"seed-{track}"
    run.mkdir()
    e = m.Emulator(a.baseline, run, a.firmware_dir, a.rom, "ASCII16", machine="Panasonic_FS-A1GT")
    state = out / f"track-{track}.oms"
    try:
        e.command("set pause on; ext Makoto")
        configure(e)
        e.command("reset")
        step(e, 15)
        e.command(f"debug write memory 0xD200 {track}; debug write memory 0xD201 {songs[track]['default_bank']-1}; keymatrixdown 8 1")
        step(e, .1)
        e.command("keymatrixup 8 1")
        step(e, .5)
        assert e.command("debug read memory 0xD203") == "0"
        e.command("store_machine [machine] " + m.tcl_path(state))
    finally:
        e.close()
    recordings = []
    for label, exe in (("baseline", a.baseline), ("candidate", a.candidate)):
        run = out / f"{label}-{track}"
        run.mkdir()
        e = m.Emulator(exe, run, a.firmware_dir, a.rom, "ASCII16", machine="Panasonic_FS-A1GT")
        try:
            e.command("set pause on; set old [machine]; set new [restore_machine " + m.tcl_path(state) + "]; delete_machine $old; activate_machine $new")
            configure(e)
            step(e, .2)
            wav = out / f"{label}-{track}.wav"
            e.command("soundlog start " + m.tcl_path(wav))
            step(e, 20)
            e.command("soundlog stop")
            with wave.open(str(wav)) as f:
                assert f.getframerate() == 44100 and f.getnchannels() == 2
                data = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").astype(float)
            recordings.append(data)
        finally:
            e.close()
    x, y = recordings
    assert len(x) == len(y), (len(x), len(y))
    delta = y-x
    row = dict(track=track+1, name=songs[track].get("display_name", songs[track]["name"]),
               max_pcm_difference=float(np.max(abs(delta))), rms_pcm_difference=float(np.sqrt(np.mean(delta*delta))),
               baseline_rms=float(np.sqrt(np.mean(x*x))), candidate_rms=float(np.sqrt(np.mean(y*y))),
               clipped_samples=int(np.count_nonzero(abs(y) >= 32767)))
    report.append(row)
    print(json.dumps(row), flush=True)
(out / "results.json").write_text(json.dumps(report, indent=2))
print(out)
