#!/usr/bin/env python3
"""Developer state restore: new ROM assets, unchanged runtime and saved Flash."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import runpy
import re
import tempfile
import time

READ_ROM_PIXEL = 'debug read memory 0x5000'

ROOT = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(ROOT / 'Contrib/rom-replacement-test.py'))
Emulator, image, tcl_path = (helpers[n] for n in ('Emulator', 'image', 'tcl_path'))

def check_rejected_states(emu, folder, rom, state):
    # Save mid-unlock, then create newer persistent progress. Rejecting
    # the busy snapshot must not flush its older Flash over that save.
    emu.command('debug write memory 0x4aaa 0xaa; savestate busy_flash; debug write memory 0x8100 0xf0')
    emu.command('debug write memory 0x4aaa 0xaa; debug write memory 0x4555 0x55; debug write memory 0x4aaa 0xa0; debug write memory 0x8101 0x3c')
    emu.advance(0.01)
    assert emu.command('debug read memory 0x8101') == '60'
    emu.command('carta eject; carta ' + tcl_path(rom) + ' -romtype ASCII16-X; reset')  # Flush progress.
    emu.advance(3)
    current = emu.command('machine')
    save_files = list((folder / 'home').rglob('*.sparse'))
    assert save_files
    disk_before = {str(p): p.read_bytes() for p in save_files}
    error = emu.command('catch {loadstate_dev busy_flash} reason; set reason')
    assert 'Flash command' in error, error
    assert emu.command('machine') == current
    assert {str(p): p.read_bytes() for p in save_files} == disk_before
    assert emu.command('debug write ioports 0xa8 0xd4; debug write memory 0x7000 8; debug read memory 0x8101') == '60'
    legacy_xml = gzip.decompress(state.read_bytes()).decode()
    legacy_xml = legacy_xml.replace('<flash version="5">', '<flash version="4">')
    legacy_xml = re.sub(r'<modifiedSectors>.*?</modifiedSectors>', '', legacy_xml, flags=re.S)
    legacy = state.with_name('legacy_flash.oms')
    legacy.write_bytes(gzip.compress(legacy_xml.encode()))
    error = emu.command('catch {loadstate_dev legacy_flash} reason; set reason')
    assert 'older states lack sector history' in error, error
    assert emu.command('machine') == current
    assert {str(p): p.read_bytes() for p in save_files} == disk_before
    print('  PASS busy/legacy state rejection retains current machine and newer persistent save', flush=True)


def run_case(args, out, name, mapper):
    folder = out / name
    folder.mkdir()
    rom = folder / ('graphics test.rom.gz' if name == 'gzip' else 'graphics test.rom')
    encode = (lambda b: gzip.compress(b, mtime=0)) if name == 'gzip' else (lambda b: b)
    rom.write_bytes(encode(image(0x11)))
    patch = folder / 'text patch.ips' if name == 'ips' else None
    if patch:
        patch.write_bytes(b'PATCH\x00\x10\x00\x00\x01\x33EOF')
    emu = Emulator(args.openmsx, folder, args.firmware_dir, rom, mapper, patch)
    try:
        assert 'loadstate_dev' in emu.command('bind CTRL+SHIFT+F7')
        # CPU waits in RAM immediately before an RLE graphics decoder.
        emu.command('debug write_block memory 0xc100 [binary format H* 18fe]; reg PC 0xc100; reg SP 0xff00; debug write ioports 0xa8 0xd4; debug write memory 0x6000 0; debug write memory 0x7000 8')
        if mapper == 'ASCII16-X':
            emu.command('debug write memory 0x4aaa 0xaa; debug write memory 0x4555 0x55; debug write memory 0x4aaa 0xa0; debug write memory 0x8100 0x5a')
            emu.advance(0.01)
            assert emu.command('debug read memory 0x8100') == '90'
        # Decoder repeats one ROM pixel four times into RAM, then returns to the wait loop.
        emu.command('debug write_block memory 0xc200 [binary format H* 3a00502100c30604772310fcc300c1]; debug write_block memory 0xc400 [binary format H* 12345678]; debug write_block VRAM 0x100 [binary format H* 2468ace0]')
        emu.advance(0.02)
        snapshot = 'list [reg PC] [reg SP] [reg AF] [debug read ioports 0xa8] [binary encode hex [debug read_block memory 0xc400 4]] [binary encode hex [debug read_block VRAM 0x100 4]]'
        runtime = emu.command(snapshot)
        state = Path(emu.command('savestate gfx_test'))
        state_hash = hashlib.sha256(state.read_bytes()).hexdigest()
        emu.command('savestate')
        if args.release_rom_before_rebuild:
            emu.command('carta eject')
        time.sleep(1.1)  # Existing file-pool hash cache uses whole-second mtimes.
        rom.write_bytes(encode(image(0x22)))
        if patch:
            patch.write_bytes(b'PATCH\x00\x10\x00\x00\x01\x44EOF')
        expected = 0x44 if patch else 0x22
        emu.command('loadstate gfx_test')
        normal = 0x22
        if mapper == 'ASCII16-X':
            normal = 0x11
        if patch:
            normal = 0x33
        assert int(emu.command(READ_ROM_PIXEL)) == normal
        assert emu.command(snapshot) == runtime
        # Development restore refreshes ROM assets but does not reset the MSX.
        emu.command('loadstate_dev gfx_test')
        assert emu.command(snapshot) == runtime
        assert int(emu.command(READ_ROM_PIXEL)) == expected
        if mapper == 'ASCII16-X':
            assert emu.command('debug read memory 0x8100') == '90'
        emu.command('reg PC 0xc200')
        emu.advance(0.02)
        assert emu.command('binary encode hex [debug read_block memory 0xc300 4]') == f'{expected:02x}' * 4
        # Default shortcut action loads quicksave, not the just-decoded frame.
        emu.command('loadstate_dev')
        assert emu.command(snapshot) == runtime
        assert int(emu.command(READ_ROM_PIXEL)) == expected
        current = emu.command('machine')
        assert emu.command('catch {loadstate_dev nonexistent_state}') == '1'
        assert emu.command('machine') == current
        # Original saved snapshot is immutable; ordinary restore still loads old Flash.
        emu.command('loadstate gfx_test')
        assert int(emu.command(READ_ROM_PIXEL)) == normal
        assert hashlib.sha256(state.read_bytes()).hexdigest() == state_hash
        assert (gzip.decompress(rom.read_bytes()) if name == 'gzip' else rom.read_bytes())[0x20100] == 255
        if name == 'flash':
            check_rejected_states(emu, folder, rom, state)
        report = 'PASS: runtime/banks/VRAM retained, new graphics decoded, Flash save preserved, ordinary restore unchanged'
        print(name, report, flush=True)
        return report
    finally:
        emu.close()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware-dir', type=Path, required=True)
    parser.add_argument('--openmsx', type=Path, default=ROOT / 'derived/x64-VC-Release/install/openmsx.exe')
    parser.add_argument('--release-rom-before-rebuild', action='store_true', help='Release the host ROM before writing on Windows without the ROM snapshot patch')
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix='dev-restore-', dir=ROOT / 'derived'))
    print('Artifacts:', out, flush=True)
    reports = {}
    for name, mapper in [('flash', 'ASCII16-X'), ('plain', 'ASCII16'), ('ips', 'ASCII16-X'), ('gzip', 'ASCII16-X')]:
        reports[name] = run_case(args, out, name, mapper)
    (out / 'results.json').write_text(json.dumps(reports, indent=2))

if __name__ == '__main__':
    main()
