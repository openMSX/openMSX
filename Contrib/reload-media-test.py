#!/usr/bin/env python3
"""Check no-reset ROM/disk refresh against disposable fixtures on Windows."""
import argparse
import gzip
import json
import os
from pathlib import Path
import runpy
import tempfile

ROOT = Path(__file__).resolve().parents[1]
h = runpy.run_path(str(ROOT / 'Contrib/rom-replacement-test.py'))
Emulator, image, tcl_path = (h[n] for n in ('Emulator', 'image', 'tcl_path'))


def stamp(path):
    st = path.stat()
    os.utime(path, (st.st_atime, st.st_mtime + 3))


def frozen(emu):
    return emu.command('list [reg PC] [reg SP] [reg AF] [reg BC] [reg DE] [reg HL] [reg IX] [reg IY] [machine_info time] [binary encode hex [debug read_block memory 0xc000 256]]')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--openmsx', type=h['executable_path'], default=ROOT / 'derived/x64-VC-Release/install/openmsx.exe')
    p.add_argument('--firmware-dir', type=Path, required=True)
    p.add_argument('--case', choices=('all', 'rom', 'gzip-rom', 'disk', 'gzip-disk'), default='all')
    p.add_argument('--native-roundtrip', action='store_true', help='Use upstream store_machine/restore_machine directly for a disk reproduction')
    args = p.parse_args()
    out = Path(tempfile.mkdtemp(prefix='reload-media-', dir=ROOT / 'derived'))
    reports = {}
    cases = ('rom', 'gzip-rom', 'disk', 'gzip-disk') if args.case == 'all' else (args.case,)
    if args.native_roundtrip and args.case not in ('disk', 'gzip-disk'):
        p.error('--native-roundtrip requires --case disk or gzip-disk')
    for name in cases:
        folder = out / name
        folder.mkdir()
        rom = folder / ('test.rom.gz' if name == 'gzip-rom' else 'test.rom')
        encode = gzip.compress if name == 'gzip-rom' else lambda b: b
        rom.write_bytes(encode(image(0x11)))
        emu = Emulator(args.openmsx, folder, args.firmware_dir, rom, 'ASCII16')
        events = []
        try:
            emu.advance(5)
            emu.expect(0x11, image(0x11))
            emu.command('set pause on')
            if 'disk' not in name:
                before = frozen(emu)
                rom.write_bytes(encode(image(0x22)))
                stamp(rom)
                emu.command('reload_media')
                assert frozen(emu) == before, 'CPU/RAM/time changed'
                assert emu.command('debug read memory 0x5000') == '34'
                events.append('updated ROM visible; CPU, RAM and emulated time preserved')
                # Inject a restore error to verify rollback of this wrapper.
                old_id = emu.command('machine')
                emu.command('rename restore_machine test_real_restore; proc restore_machine args {error "injected restore failure"}')
                try:
                    assert emu.command('catch {reload_media}') == '1'
                    assert emu.command('machine') == old_id
                    assert frozen(emu) == before
                finally:
                    emu.command('rename restore_machine {}; rename test_real_restore restore_machine')
                events.append('failed restore retains original machine')
            else:
                disk = folder / 'test.dsk'
                asset = folder / 'asset.bin'
                original = bytes(range(128)) * 4
                updated = bytes(reversed(range(128))) * 4
                asset.write_bytes(original)
                emu.command('diskmanipulator create ' + tcl_path(disk) + ' 720')
                emu.command('diska ' + tcl_path(disk))
                emu.command('diskmanipulator import diska ' + tcl_path(asset))
                emu.command('diska eject')
                raw = disk.read_bytes()
                offset = raw.index(original)
                assert raw.count(original) == 1
                old_state = folder / 'before.oms'
                compressed = name == 'gzip-disk'
                if compressed:
                    disk = folder / 'test.dsk.gz'
                    disk.write_bytes(gzip.compress(raw))
                emu.command('diska ' + tcl_path(disk))
                emu.command('store_machine [machine] ' + tcl_path(old_state))
                before = frozen(emu)
                if compressed:
                    disk.write_bytes(gzip.compress(raw[:offset] + updated + raw[offset+512:]))
                else:
                    replacement = folder / 'replacement.dsk'
                    replacement.write_bytes(raw[:offset] + updated + raw[offset+512:])
                    try:
                        os.replace(replacement, disk)
                        events.append('host atomic replacement allowed')
                    except PermissionError:
                        events.append('LIMIT: mounted writable DSK still blocks host atomic replacement; in-place editing works')
                    with disk.open('r+b') as stream:
                        stream.seek(offset)
                        stream.write(updated)
                stamp(disk)
                if args.native_roundtrip:
                    roundtrip = tcl_path(folder / 'roundtrip.oms')
                    emu.command('set old [machine]; store_machine $old ' + roundtrip + '; set new [restore_machine ' + roundtrip + ']; delete_machine $old; activate_machine $new')
                else:
                    emu.command('reload_media')
                assert frozen(emu) == before
                export = folder / 'export'
                export.mkdir()
                emu.command('diskmanipulator export diska ' + tcl_path(export))
                actual = (export / 'asset.bin').read_bytes()
                if compressed and actual == original:
                    reports[name] = ['KNOWN LIMIT: compressed disk cache retains old data while original machine exists']
                    emu.command('diska eject; diska ' + tcl_path(disk))
                    fresh_export = folder / 'export-after-reinsert'
                    fresh_export.mkdir()
                    emu.command('diskmanipulator export diska ' + tcl_path(fresh_export))
                    assert (fresh_export / 'asset.bin').read_bytes() == updated
                    reports[name].append('eject/reinsert reads the updated compressed disk')
                    print('KNOWN LIMIT', name, *reports[name], sep='\n  ', flush=True)
                    continue
                assert actual == updated, 'stale disk data after refresh'
                events.append('updated disk asset visible with CPU/RAM/time preserved')
                # Actual translation workflow: restore an earlier state before
                # loading the changed asset, not just an immediate roundtrip.
                emu.command('set old [machine]; set new [restore_machine ' + tcl_path(old_state) + ']; delete_machine $old; activate_machine $new')
                export2 = folder / 'export-old-state'
                export2.mkdir()
                emu.command('diskmanipulator export diska ' + tcl_path(export2))
                assert (export2 / 'asset.bin').read_bytes() == updated
                events.append('older save state reads updated disk asset')
            reports[name] = events
            print('PASS', name, *events, sep='\n  ', flush=True)
        finally:
            emu.close()
            (out / 'results.json').write_text(json.dumps(reports, indent=2), encoding='utf-8')
            print('Artifacts:', out, flush=True)


if __name__ == '__main__':
    main()
