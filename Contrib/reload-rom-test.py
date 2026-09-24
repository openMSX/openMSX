#!/usr/bin/env python3
"""Test reload_rom, its default hotkey, mapper/IPS retention and file validation."""
import argparse
import gzip
import json
import runpy
from pathlib import Path
import tempfile
import time

MEDIA_CARTA = 'machine_info media carta'
CATCH_RELOAD = 'catch {reload_rom}'

ROOT = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(ROOT / 'Contrib/rom-replacement-test.py'))
Emulator, image, tcl_path = (helpers[n] for n in ('Emulator', 'image', 'tcl_path'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware-dir', type=Path, required=True)
    parser.add_argument('--openmsx', type=Path, default=ROOT / 'derived/x64-VC-Release/install/openmsx.exe')
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix='reload-rom-', dir=ROOT / 'derived'))
    results = {}
    for name, mapper in [('ascii16', 'ASCII16'), ('ips', 'ASCII16'), ('gzip', 'ASCII16')]:
        folder = out / name
        folder.mkdir()
        rom = folder / ('developer image.rom.gz' if name == 'gzip' else 'developer image.rom')
        encode = (lambda b: gzip.compress(b, mtime=0)) if name == 'gzip' else (lambda b: b)
        original, rebuilt = image(0x11), image(0x22)
        rom.write_bytes(encode(original))
        ips = None
        old_view, new_view = bytearray(original), bytearray(rebuilt)
        marker_before, marker_after = 0x11, 0x22
        if name == 'ips':
            ips = folder / 'developer patch.ips'
            ips.write_bytes(b'PATCH\x00\x10\x00\x00\x01\x33EOF')
            old_view[0x1000] = new_view[0x1000] = marker_before = marker_after = 0x33
        emu = Emulator(args.openmsx, folder, args.firmware_dir, rom, mapper, ips)
        events = []
        try:
            assert 'reload_rom' in emu.command('bind CTRL+SHIFT+R'), 'Hotkey not active'
            emu.expect(marker_before, original, bytes(old_view))
            old_media = emu.command(MEDIA_CARTA)
            # Bad/missing build outputs must leave the existing cartridge inserted.
            rom.unlink()
            assert emu.command(CATCH_RELOAD) == '1'
            assert emu.command(MEDIA_CARTA) == old_media
            rom.write_bytes(b'')
            assert emu.command(CATCH_RELOAD) == '1'
            assert emu.command(MEDIA_CARTA) == old_media
            # The existing ROM hash cache compares timestamps at one-second precision.
            time.sleep(1.1)
            rom.write_bytes(encode(rebuilt))
            events.append('missing/empty file retains the running cartridge')
            emu.command('reset')
            emu.advance(3)
            emu.expect(marker_before, original, bytes(old_view))
            events.append('ordinary reset retains the loaded snapshot')

            # Run precisely the action wired to the shortcut, without GUI input.
            emu.command('reload_rom')
            emu.advance(3)
            emu.expect(marker_after, rebuilt, bytes(new_view))
            assert emu.command('dict get [machine_info media carta] mappertype') == mapper
            if ips:
                assert emu.command('lindex [dict get [machine_info media carta] patches] 0').replace('\\', '/') == ips.as_posix()
            events.append('reload/reset boots rebuilt bytes and retains mapper/IPS and hashes')

            # Explicit cartridge selection, including a ROM in the second slot.
            emu.command('carta eject')
            assert emu.command(CATCH_RELOAD) == '1'
            command = 'cartb ' + tcl_path(rom) + ' -romtype ' + mapper
            if ips:
                command += ' -ips ' + tcl_path(ips)
            emu.command(command)
            emu.command('reload_rom cartb')
            emu.advance(3)
            emu.expect(marker_after, rebuilt, bytes(new_view))
            assert emu.command('dict get [machine_info media cartb] mappertype') == mapper
            emu.command('carta ' + tcl_path(rom) + ' -romtype ASCII16')
            assert emu.command(CATCH_RELOAD) == '1', 'Ambiguous ROMs were silently selected'
            events.append('empty-slot error, slot B reload and ambiguous-slot handling')
            results[name] = events
            print('PASS', name, *events, sep='\n  ', flush=True)
        finally:
            emu.close()
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('Artifacts:', out)


if __name__ == '__main__':
    main()
