"""Count actual FM/SSG/rhythm/ADPCM-B writes from each music-ROM track."""
import argparse
import hashlib
import importlib.util
import json
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('makotest', ROOT / 'Contrib/makoto-test.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
p = argparse.ArgumentParser(__doc__)
p.add_argument('--openmsx', type=Path, required=True)
p.add_argument('--firmware-dir', type=Path, required=True)
p.add_argument('--rom', type=Path, required=True)
p.add_argument('--manifest', type=Path, required=True)
p.add_argument('--seconds', type=float, default=20)
a = p.parse_args()
out = Path(tempfile.mkdtemp(prefix='makoto-coverage-', dir=ROOT / 'derived'))
e = m.Emulator(a.openmsx, out, a.firmware_dir, a.rom, 'ASCII16', machine='Panasonic_FS-A1GT')
report = {'rom_sha256': hashlib.sha256(a.rom.read_bytes()).hexdigest(), 'seconds_per_track': a.seconds, 'tracks': []}

def step(seconds):
    e.command(f'after time {seconds} {{set pause on}}; set pause off')
    deadline = time.monotonic() + 120
    while e.command('set pause') != 'true':
        assert time.monotonic() < deadline

try:
    e.command('set pause on; ext Makoto; reset')
    step(15)
    e.command('''set coverage_latch 0
array set coverage {}
proc coverage_write {} {
    set port [expr {$::wp_last_address & 3}]
    set value $::wp_last_value
    if {$port == 0 || $port == 2} {
        set ::coverage_latch [expr {$value + ($port == 2 ? 256 : 0)}]
        return
    }
    set reg $::coverage_latch
    incr ::coverage($reg)
    if {$reg == 16 && !($value & 128) && ($value & 63)} {incr ::coverage(rhythm_keyon)}
    if {$reg == 256 && ($value & 128) && !($value & 64)} {incr ::coverage(adpcm_b_start)}
}
set coverage_wp [debug set_watchpoint write_io {0x14 0x17} {} coverage_write]
''')
    for song in json.loads(a.manifest.read_text())['songs']:
        e.command(f"debug write memory 0xD200 {song['index']-1}; debug write memory 0xD201 {song['default_bank']-1}; array unset coverage; keymatrixdown 8 1")
        step(.1)
        e.command('keymatrixup 8 1')
        step(a.seconds)
        assert e.command('debug read memory 0xD203') == '0'
        raw = e.command('array get coverage').split()
        writes = dict(zip(raw[::2], map(int, raw[1::2])))
        row = {'track': song['index'], 'name': song.get('display_name', song['name']), 'writes': writes,
               'rhythm_keyons': writes.get('rhythm_keyon', 0), 'adpcm_b_starts': writes.get('adpcm_b_start', 0)}
        report['tracks'].append(row)
        (out / 'results.json').write_text(json.dumps(report, indent=2))
        print(row['track'], row['name'], row['rhythm_keyons'], row['adpcm_b_starts'], flush=True)
finally:
    e.close()
    print(out)
