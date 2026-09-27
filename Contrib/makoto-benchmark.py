"""Paired host-CPU benchmark and exact WAV/state comparison for Makoto builds.

Needs NumPy and the same firmware files as makoto-test.py. CPU timing uses
GetProcessTimes on Windows; wall timing is reported on all platforms.
"""
import argparse
import ctypes
import gzip
import importlib.util
import json
import os
import statistics
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


def cpu_time(process):
    if os.name != 'nt':
        return None
    f = ctypes.WinDLL('kernel32', use_last_error=True).GetProcessTimes
    f.argtypes = [ctypes.c_void_p] + [ctypes.POINTER(ctypes.c_uint64)] * 4
    f.restype = ctypes.c_int
    values = [ctypes.c_uint64() for _ in range(4)]
    assert f(int(process._handle), *(ctypes.byref(v) for v in values))
    return (values[2].value + values[3].value) * 1e-7


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--firmware-dir', type=Path, required=True)
    p.add_argument('--seconds', type=float, default=20)
    p.add_argument('--repeats', type=int, default=3)
    a = p.parse_args()
    out = Path(tempfile.mkdtemp(prefix='makoto-bench-', dir=ROOT / 'derived'))
    rom = out / 'test.rom'
    rom.write_bytes(m.image(0x40))
    emulators = {}
    results = {'emulated_seconds_per_run': a.seconds, 'runs': [], 'audio': {}}

    def step(e, seconds):
        e.command(f'after time {seconds} {{set pause on}}; set pause off')
        until = time.monotonic() + 120
        while e.command('set pause') != 'true':
            assert time.monotonic() < until

    def write(e, reg, value):
        port = 22 if reg >= 256 else 20
        e.command(f'debug write ioports {port} {reg & 255}; debug write ioports {port + 1} {value}')

    def restore(e):
        e.command('set old [machine]; set new [restore_machine ' + m.tcl_path(out / 'fixture.oms') +
                  ']; delete_machine $old; activate_machine $new; set pause on; set mute off; '
                  'set Makoto_volume 20; set makoto_psg_volume 50')

    def sound_state(path):
        root = ET.fromstring(gzip.decompress(path.read_bytes()))
        return ET.tostring(root.find('.//device[@type="Makoto"]/sound'))

    try:
        for label, exe in (('baseline', a.baseline), ('candidate', a.candidate)):
            directory = out / label
            directory.mkdir()
            e = m.Emulator(exe, directory, a.firmware_dir, rom, 'ASCII16')
            emulators[label] = e
            e.command('set pause on; ext Makoto; set mute off; set volume 50; set Makoto_volume 20')
        e = emulators['baseline']
        write(e, 0x29, 0x80)
        for c in range(6):
            reg = (c // 3) * 256 + c % 3
            for slot in (0, 4, 8, 12):
                for base, value in ((0x30, 1), (0x40, 40), (0x50, 31), (0x60, 0), (0x70, 0), (0x80, 15)):
                    write(e, reg + slot + base, value)
            write(e, reg + 0xb0, 7)
            write(e, reg + 0xb4, 0xc0)
            write(e, reg + 0xa4, 0x22)
            write(e, reg + 0xa0, 0x60 + c * 10)
            write(e, 0x28, 0xf0 + (c // 3) * 4 + c % 3)
        for c in range(3):
            write(e, c * 2, 37 + c * 24)
            write(e, c * 2 + 1, 1)
            write(e, 8 + c, 12)
        write(e, 6, 3)
        write(e, 7, 0)
        write(e, 0x11, 48)
        for c in range(6):
            write(e, 0x18 + c, 0xdf)
        write(e, 0x10, 0x3f)
        e.command('store_machine [machine] ' + m.tcl_path(out / 'fixture.oms'))
        for mode in ('combined', 'channel-tools'):
            audio = {}
            states = {}
            for label, e in emulators.items():
                restore(e)
                if mode == 'channel-tools':
                    e.command('set Makoto_ch1_record ' + m.tcl_path(out / label / 'fm1.wav'))
                wav_path = out / label / (mode + '.wav')
                e.command('soundlog start ' + m.tcl_path(wav_path))
                for reg, gain in ((0x2d, 50), (0x2e, 75), (0x2f, 0), (0x2d, 50)):
                    e.command(f'debug write ioports 20 {reg}; set makoto_psg_volume {gain}')
                    step(e, .1)
                e.command('soundlog stop; set Makoto_ch1_record {}')
                with wave.open(str(wav_path)) as f:
                    audio[label] = np.frombuffer(f.readframes(f.getnframes()), dtype='<i2')
                state_path = out / label / (mode + '.oms')
                e.command('store_machine [machine] ' + m.tcl_path(state_path))
                states[label] = sound_state(state_path)
            assert np.array_equal(audio['baseline'], audio['candidate']), (mode, 'WAV output differs')
            assert states['baseline'] == states['candidate'], (mode, 'saved chip state differs')
            results['audio'][mode] = f'{len(audio["baseline"])} PCM values and full saved chip state match exactly'
            for repeat in range(a.repeats):
                order = ('baseline', 'candidate') if repeat % 2 == 0 else ('candidate', 'baseline')
                for label in order:
                    e = emulators[label]
                    restore(e)
                    if mode == 'channel-tools':
                        # A mute forces independent voice buffers without disk-recording cost.
                        e.command('set Makoto_ch1_mute true')
                    step(e, .1)
                    cpu = cpu_time(e.process)
                    wall = time.perf_counter()
                    step(e, a.seconds)
                    wall = time.perf_counter() - wall
                    after_cpu = cpu_time(e.process)
                    row = {'mode': mode, 'build': label, 'repeat': repeat, 'wall_seconds': wall,
                           'cpu_seconds': None if cpu is None else after_cpu - cpu}
                    results['runs'].append(row)
                    print(json.dumps(row), flush=True)
        results['summary'] = {}
        for mode in ('combined', 'channel-tools'):
            medians = {}
            for label in emulators:
                rows = [r for r in results['runs'] if r['mode'] == mode and r['build'] == label]
                key = 'cpu_seconds' if rows[0]['cpu_seconds'] is not None else 'wall_seconds'
                medians[label] = statistics.median(r[key] for r in rows)
            results['summary'][mode] = {'median_seconds': medians,
                'reduction_percent': 100 * (1 - medians['candidate'] / medians['baseline'])}
    finally:
        for e in emulators.values():
            e.close()
        (out / 'results.json').write_text(json.dumps(results, indent=2))
    print(out)
    print(json.dumps(results['audio'], indent=2))
    print(json.dumps(results['summary'], indent=2))


if __name__ == '__main__':
    main()
