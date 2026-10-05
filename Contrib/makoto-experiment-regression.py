"""Regression checks for the experimental YM2608 integration."""
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


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--openmsx', type=Path, required=True)
    p.add_argument('--firmware-dir', type=Path, required=True)
    a = p.parse_args()
    out = Path(tempfile.mkdtemp(prefix='makoto-experiment-', dir=ROOT / 'derived'))
    rom = out / 'test.rom'
    rom.write_bytes(m.image(0x41))
    e = m.Emulator(a.openmsx, out, a.firmware_dir, rom, 'ASCII16')
    results = {}

    def write(reg, value):
        port = 22 if reg >= 256 else 20
        e.command(f'debug write ioports {port} {reg & 255}; debug write ioports {port+1} {value}')

    def step(seconds):
        e.command(f'after time {seconds} {{set pause on}}; set pause off')
        deadline = time.monotonic() + 30
        while e.command('set pause') != 'true':
            assert time.monotonic() < deadline

    def tone(label):
        path = out / (label + '.wav')
        e.command('soundlog start ' + m.tcl_path(path))
        step(.15)
        e.command('soundlog stop')
        with wave.open(str(path)) as w:
            rate = w.getframerate()
            x = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, 2)[:, 0]
        x = x[len(x)//3:].astype(float)
        x -= np.mean(x)
        edges = np.flatnonzero((x[1:] >= 0) & (x[:-1] < 0))
        assert len(edges) > 10, (label, len(edges))
        return rate / float(np.mean(np.diff(edges)))

    try:
        e.command('set pause on; ext Makoto; set mute off; set volume 50; set {Makoto SSG_volume} 30')
        write(7, 0x38)
        write(8, 0)
        e.command('debug write ioports 20 7; debug write {Makoto registers} 8 15')
        regs = [int(e.command(f'debug read {{Makoto registers}} {r}')) for r in (7, 8)]
        e.command('debug write ioports 21 0x39')
        latch_ok = int(e.command('debug read {Makoto registers} 7')) == 0x39
        results['debugger_register_and_latch'] = {'values': regs, 'latch_preserved': latch_ok, 'pass': regs == [0x38, 15] and latch_ok}
        write(0, 128)
        write(1, 0)
        write(7, 0x3e)
        write(8, 15)
        rates = []
        for address in (0x2d, 0x2e, 0x2f):
            e.command(f'debug write ioports 20 {address}')
            step(.02)
            rates.append(tone(f'prescale-{address:x}'))
        expected = [976.5625, 1953.125, 3906.25]
        results['ssg_prescale_pitch'] = {'actual_hz': rates, 'expected_hz': expected, 'pass': all(abs(x-y)/y < .01 for x,y in zip(rates,expected))}
        for prescale, address in ((6, 0x2d), (3, 0x2e), (2, 0x2f)):
            e.command('debug write ioports 20 45')
            e.command(f'debug write ioports 20 {address}')
            saved = out / f'prescale-{prescale}.oms'
            e.command('store_machine [machine] ' + m.tcl_path(saved))
            e.command('set old [machine]; set new [restore_machine ' + m.tcl_path(saved) + ']; delete_machine $old; activate_machine $new; set pause on')
            hz = tone(f'restored-{prescale}')
            target = expected[(6, 3, 2).index(prescale)]
            results[f'restored_pitch_{prescale}'] = {'hz': hz, 'pass': abs(hz-target)/target < .01}
    finally:
        e.close()
        (out / 'results.json').write_text(json.dumps(results, indent=2))
        print(out)
        print(json.dumps(results, indent=2))
    assert all(r['pass'] for r in results.values()), 'Regression detected'


if __name__ == '__main__':
    main()
