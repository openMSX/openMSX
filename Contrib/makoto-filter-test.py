"""Record deterministic Makoto filter A/B clips and benchmark the paired builds.

The optional music ROM is local-only. All outputs and the blind listening pack
stay under derived; no game assets are part of the repository.
"""
import argparse
import gzip
import importlib.util
import json
from pathlib import Path
import shutil
import statistics
import tempfile
import time
import wave
import xml.etree.ElementTree as ET

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result
m = module('makotest', ROOT/'Contrib/makoto-test.py')
b = module('makobench', ROOT/'Contrib/makoto-benchmark.py')


def step(e, seconds):
    e.command(f'after time {seconds} {{set pause on}}; set pause off')
    until = time.monotonic() + 120
    while e.command('set pause') != 'true':
        assert time.monotonic() < until


def write(e, reg, value):
    port = 22 if reg >= 256 else 20
    e.command(f'debug write ioports {port} {reg & 255}; debug write ioports {port+1} {value}')


def restore(e, path):
    e.command('set old [machine]; set new [restore_machine '+m.tcl_path(path)+
              ']; delete_machine $old; activate_machine $new; set pause on; '
              'set mute off; set volume 100; set Makoto_volume 20; set [lindex [info vars ?akoto_psg_volume] 0] 50')


def save(e, path):
    e.command('store_machine [machine] '+m.tcl_path(path))


def data(path):
    with wave.open(str(path)) as f:
        assert f.getsampwidth() == 2 and f.getnchannels() == 2
        return f.getframerate(), np.frombuffer(f.readframes(f.getnframes()), '<i2').reshape(-1, 2).astype(float)


def compare(a, z):
    fs, x = data(a)
    fs2, y = data(z)
    assert fs == fs2 and x.shape == y.shape, (fs, fs2, x.shape, y.shape)
    assert np.max(abs(x)) < 32767 and np.max(abs(y)) < 32767, 'Clipped recording'
    # Exclude the initial host-resampler transient. Do not time-align or level-match.
    x, y = x[int(fs*.1):], y[int(fs*.1):]
    rms = lambda q: float(np.sqrt(np.mean(q*q)))
    return {'rate': fs, 'frames': len(x), 'baseline_rms': rms(x), 'candidate_rms': rms(y),
            'level_change_db': 20*np.log10(rms(y)/rms(x)),
            'difference_relative_db': 20*np.log10(max(rms(y-x), 1e-20)/rms(x)),
            'peak': [float(np.max(abs(x))), float(np.max(abs(y)))]}


def compare_state(first, second):
    a = ET.fromstring(gzip.decompress(first.read_bytes())).find('.//device[@type="Makoto"]/sound')
    z = ET.fromstring(gzip.decompress(second.read_bytes())).find('.//device[@type="Makoto"]/sound')
    for name in ('core', 'busyEnd', 'sampleRAM', 'irq', 'sampleClock', 'timerA', 'timerB'):
        assert ET.tostring(a.find(name)) == ET.tostring(z.find(name)), 'State changed: '+name
    # SSG entries 12..17 were host mixer scratch space; the unfiltered combined
    # path no longer populates them. SSG hardware state is in the compared core.
    av=[n.text for n in a.find('channelOutput')]; zv=[n.text for n in z.find('channelOutput')]
    assert av[:12]+av[18:] == zv[:12]+zv[18:], 'FM/ADPCM voice cache changed'
    return 'Exact core, RAM, timers, IRQ, BUSY, sample clock and FM/ADPCM voice cache match'


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--firmware-dir', type=Path, required=True)
    p.add_argument('--music-rom', type=Path)
    p.add_argument('--seconds', type=float, default=30)
    p.add_argument('--repeats', type=int, default=5)
    a = p.parse_args()
    if a.seconds <= 0 or a.repeats < 1:p.error('seconds and repeats must be positive')
    out = Path(tempfile.mkdtemp(prefix='makoto-filter-', dir=ROOT/'derived'))
    report = (out/'results.json').open('x', encoding='utf-8')
    rom = out/'synthetic.rom'; rom.write_bytes(m.image(0x41))
    results = {'audio': {}, 'runs': [], 'summary': {}}
    emulators = {}
    try:
        for label, exe in (('baseline', a.baseline), ('candidate', a.candidate)):
            d = out/label; d.mkdir()
            e = m.Emulator(exe, d, a.firmware_dir, rom, 'ASCII16',
                           machine='Panasonic_FS-A1GT' if a.music_rom else 'Philips_NMS_8250')
            emulators[label] = e
            e.command('set pause on; ext Makoto; set mute off; set volume 100; set Makoto_volume 20; set [lindex [info vars ?akoto_psg_volume] 0] 50')
            if a.music_rom:
                for name in ('fs-a1gt_firmware.rom', 'fs-a1gt_kanjifont.rom'):
                    shutil.copy2(a.firmware_dir/name, d/'home/share/systemroms'/name)
        e = emulators['baseline']
        clean = out/'clean.oms'; save(e, clean)
        # Constant bright square wave; harmonics exercise the audible top end.
        write(e, 0, 35); write(e, 1, 0); write(e, 7, 0x3e); write(e, 8, 13)
        fixtures = [('bright-ssg', 5, out/'bright-ssg.oms')]
        save(e, fixtures[-1][2])
        restore(e, clean)
        write(e, 0x11, 0x3f); write(e, 0x1a, 0xdf); write(e, 0x1b, 0xdf)
        fixtures += [('cymbal-hihat', 5, out/'cymbal-hihat.oms')]
        save(e, fixtures[-1][2])
        restore(e, clean)
        write(e, 0x29, 0x80)
        for c in range(6):
            reg = (c//3)*256+c%3
            for slot in (0, 4, 8, 12):
                for base, value in ((0x30,1),(0x40,40),(0x50,31),(0x60,0),(0x70,0),(0x80,15)):
                    write(e, reg+slot+base, value)
            for base, value in ((0xb0,7),(0xb4,0xc0),(0xa4,0x22),(0xa0,0x60+c*10)):
                write(e, reg+base, value)
            write(e, 0x28, 0xf0+(c//3)*4+c%3)
        for c in range(3):
            write(e,c*2,37+c*24); write(e,c*2+1,1); write(e,8+c,12)
        write(e,6,3); write(e,7,0)
        write(e,0x11,48)
        for c in range(6): write(e,0x18+c,0xdf)
        write(e,0x10,0x3f)
        stress = out/'stress.oms'; save(e,stress)
        if a.music_rom:
            e.command('carta eject')
            e.command('carta '+m.tcl_path(a.music_rom.resolve())+' -romtype ASCII16-X')
            e.command('reset; set power on; set pause on; set mute off; set volume 100; set Makoto_volume 20; set [lindex [info vars ?akoto_psg_volume] 0] 50')
            step(e,20)
            assert e.command('get_active_cpu').lower() == 'r800'
            e.command('debug write memory 0xd200 9; debug write memory 0xd201 4; keymatrixdown 8 1')
            step(e,.1); e.command('keymatrixup 8 1'); step(e,2)
            assert e.command('debug read memory 0xd202') == '1'
            fixtures += [('bustling-town', 24, out/'bustling-town.oms')]
            save(e,fixtures[-1][2])
        for name, seconds, fixture in fixtures:
            states = []
            for label,e in emulators.items():
                restore(e,fixture)
                if name == 'bright-ssg':e.command('set Makoto_volume 100')
                wav = out/label/(name+'.wav')
                e.command('soundlog start '+m.tcl_path(wav))
                if name == 'cymbal-hihat':
                    for _ in range(10):
                        write(e,0x10,0x0c); step(e,.5)
                else: step(e,seconds)
                e.command('soundlog stop')
                state = out/label/(name+'.oms'); save(e,state); states.append(state)
                if name == 'bustling-town':
                    results[label+'_town_late_ticks'] = e.command('expr {[debug read memory 0xd208]+256*[debug read memory 0xd209]}')
            results['audio'][name] = compare(out/'baseline'/(name+'.wav'),out/'candidate'/(name+'.wav'))
            results['audio'][name]['state'] = compare_state(*states)
            print('Recorded '+name, flush=True)
        # Sequential paired runs; reverse order each repetition.
        for mode in ('combined','channel-tools'):
            for repeat in range(a.repeats):
                order = ('baseline','candidate') if repeat%2==0 else ('candidate','baseline')
                for label in order:
                    e=emulators[label]; restore(e,stress)
                    if mode=='channel-tools':e.command('set Makoto_ch1_mute true')
                    step(e,.1)
                    cpu=b.cpu_time(e.process); wall=time.perf_counter()
                    step(e,a.seconds)
                    wall=time.perf_counter()-wall; after=b.cpu_time(e.process)
                    row={'mode':mode,'build':label,'repeat':repeat,'wall_seconds':wall,
                         'cpu_seconds':None if cpu is None else after-cpu}
                    results['runs'].append(row);print(json.dumps(row),flush=True)
            medians={}
            for label in emulators:
                rows=[r for r in results['runs'] if r['mode']==mode and r['build']==label]
                key='cpu_seconds' if rows[0]['cpu_seconds'] is not None else 'wall_seconds'
                medians[label]=statistics.median(r[key] for r in rows)
            results['summary'][mode]={'median_seconds':medians,'reduction_percent':100*(1-medians['candidate']/medians['baseline'])}
    finally:
        try:
            for e in emulators.values():e.close()
        finally:
            with report:json.dump(results,report,indent=2)
    print(out,flush=True)
    print(json.dumps(results['summary'],indent=2))

if __name__=='__main__':main()
