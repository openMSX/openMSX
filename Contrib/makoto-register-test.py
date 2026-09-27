"""Check debugger edits against actual YM2608 port-write semantics."""
import argparse
import gzip
import importlib.util
import json
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('makotest', ROOT / 'Contrib/makoto-test.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def main():
    p = argparse.ArgumentParser(__doc__)
    p.add_argument('--openmsx', type=Path, required=True)
    p.add_argument('--firmware-dir', type=Path, required=True)
    a = p.parse_args()
    out = Path(tempfile.mkdtemp(prefix='makoto-registers-', dir=ROOT / 'derived'))
    rom = out / 'test.rom'
    rom.write_bytes(m.image(0x39))
    e = m.Emulator(a.openmsx, out, a.firmware_dir, rom, 'ASCII16')
    passed = []

    def save(path):
        e.command('store_machine [machine] ' + m.tcl_path(path))
        node = ET.fromstring(gzip.decompress(path.read_bytes())).find('.//device[@type="Makoto"]/sound')
        return ET.tostring(node)

    def restore(path):
        e.command('set old [machine]; set new [restore_machine ' + m.tcl_path(path) +
                  ']; delete_machine $old; activate_machine $new; set pause on')

    try:
        e.command('set pause on; ext Makoto')
        # Seed both ADPCM-B write-transfer mode and timer-A settings.
        for reg, value in ((0x101, 0xc0), (0x102, 0), (0x103, 0), (0x100, 0x60),
                           (0x24, 0xff), (0x25, 3), (0x11, 0x3f), (0x18, 0xdf)):
            port = 22 if reg >= 256 else 20
            e.command(f'debug write ioports {port} {reg & 255}; debug write ioports {port + 1} {value}')
        # Full-state equality also checks RAM, BUSY, IRQ and pending timers.
        for reg, value in ((8, 15), (0xa4, 0x22), (0xa0, 0x69), (0x1b4, 0xc0),
                           (0x108, 0x5a), (0x10, 1), (0x27, 5), (0x2d, 0),
                           (0x2e, 0), (0x2f, 0)):
            for original_latch in (0x11, 0x1b4):
                latch_port = 22 if original_latch >= 256 else 20
                e.command(f'debug write ioports {latch_port} {original_latch & 255}')
                seed = out / 'seed.oms'
                save(seed)
                e.command(f'debug write {{Makoto registers}} {reg} {value}')
                actual = save(out / 'edited.oms')
                restore(seed)
                port = 22 if reg >= 256 else 20
                e.command(f'debug write ioports {port} {reg & 255}; debug write ioports {port + 1} {value}; '
                          f'debug write ioports {latch_port} {original_latch & 255}')
                expected = save(out / 'ports.oms')
                assert actual == expected, (hex(reg), hex(original_latch))
            passed.append(f'{reg:03X}h: debugger edit matches port writes; both bank address latches preserved')
    finally:
        e.close()
    (out / 'results.json').write_text(json.dumps({'passed': passed}, indent=2))
    print(out)
    print('\n'.join(passed))


if __name__ == '__main__':
    main()
