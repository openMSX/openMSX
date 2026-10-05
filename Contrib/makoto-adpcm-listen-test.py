"""Verify every portable listening case, ADPCM-B pan, EOS, repeat and restore."""
import argparse
import gzip
import importlib.util
import json
import tempfile
import time
import wave
import xml.etree.ElementTree as ET
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('makotest', ROOT / 'Contrib/makoto-test.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
p = argparse.ArgumentParser(__doc__)
p.add_argument('--openmsx', type=Path, required=True)
p.add_argument('--firmware-dir', type=Path, required=True)
a = p.parse_args()
out = Path(tempfile.mkdtemp(prefix='makoto-listening-', dir=ROOT / 'derived'))
rom = out / 'test.rom'
rom.write_bytes(m.image(0x42))
e = m.Emulator(a.openmsx, out, a.firmware_dir, rom, 'ASCII16')
report = {}

def step(seconds):
    e.command(f'after time {seconds} {{set pause on}}; set pause off')
    deadline = time.monotonic() + 30
    while e.command('set pause') != 'true':
        assert time.monotonic() < deadline

def record(case, pan='stereo', seconds=.6):
    e.command('makoto_adpcm_test 0')
    step(.1)
    path = out / f'case-{case}-{pan}.wav'
    e.command('soundlog start ' + m.tcl_path(path))
    name = e.command(f'makoto_adpcm_test {case} {pan}')
    step(seconds)
    e.command('soundlog stop')
    with wave.open(str(path)) as w:
        samples = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, 2).astype(float)
    peaks = np.max(abs(samples), axis=0)
    assert max(peaks) > 20 and max(peaks) < 32767, (name, peaks)
    if pan == 'stereo':
        assert np.array_equal(samples[:, 0], samples[:, 1]), name
    else:
        assert peaks[1 if pan == 'left' else 0] <= 1, (name, peaks)
    status = int(e.command('debug read ioports 22'))
    report[f'{case}-{pan}'] = {'name': name, 'peak': peaks.tolist(), 'status': status}
    return samples, status

def save(name):
    path = out / f'{name}.oms'
    e.command('store_machine [machine] ' + m.tcl_path(path))
    return path, ET.tostring(m.sound_node(ET.fromstring(gzip.decompress(path.read_bytes()))))

try:
    e.command('set pause on; ext Makoto; set mute off; set volume 50; set Makoto_volume 20')
    e.command('source ' + m.tcl_path(ROOT / 'Contrib/makoto-adpcm-listen.tcl'))
    for case in range(1, 9):
        samples, status = record(case)
        if case == 7:
            assert status & 4 and not status & 32, status
            assert np.max(abs(samples[-2048:])) <= 1
        if case == 8:
            assert status & 32 and not status & 4, status
            assert np.sqrt(np.mean(samples[-2048:]**2)) > 20
    record(8, 'left')
    record(8, 'right')
    seed, _ = save('seed')
    step(.123)
    _, expected = save('expected')
    e.command('set old [machine]; set new [restore_machine ' + m.tcl_path(seed) + ']; delete_machine $old; activate_machine $new; set pause on')
    step(.123)
    _, actual = save('actual')
    assert actual == expected, 'ADPCM-B continuation diverged'
    e.command('reverse start')
    step(.2)
    target = e.command('machine_info time')
    _, expected = save('rewind-before')
    step(.1)
    e.command('reverse goto ' + target)
    _, actual = save('rewind-after')
    assert actual == expected, 'ADPCM-B rewind diverged'
    report['continuation'] = 'Exact complete chip state after save/restore and native rewind during ADPCM-B repeat playback'
    e.command('reverse stop; makoto_adpcm_test 0')
finally:
    e.close()
    (out / 'results.json').write_text(json.dumps(report, indent=2))
    print(out)
    print(json.dumps(report, indent=2))
