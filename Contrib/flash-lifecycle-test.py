#!/usr/bin/env python3
"""Firmware-free Flash save retry, reverse and replay regression tests."""
import argparse
import gzip
import json
from pathlib import Path
import runpy
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(ROOT/'Contrib/rom-test-utils.py'))
formats = runpy.run_path(str(ROOT/'Contrib/flash-persistence-test.py'))
Emulator, image, tcl_path = (helpers[n] for n in ('Emulator', 'image', 'tcl_path'))


def wait_for(predicate, description, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.05)
    raise AssertionError('Timed out: ' + description)


def program(emu, address, value):
    bank, offset = address >> 14, address & 0x3fff
    emu.command(f'debug write memory {0x6000 | (bank & 0xf00)} {bank & 255}; '
                'debug write memory 0x4aaa 0xaa; debug write memory 0x4555 0x55; '
                f'debug write memory 0x4aaa 0xa0; debug write memory {0x4000 | offset} {value}')
    emu.advance(0.01)


def read(emu, address):
    # Read the physical Flash debuggable without recording a mapper write.
    # A debug write after rewinding would fork and truncate the replay history.
    return int(emu.command(f'debug read [lsearch -inline -glob [debug list] *_flash] {address}'))


def idle_in_ram(emu):
    emu.command('debug write_block memory 0xc100 [binary format H* f3c301c1]; '
                'reg PC 0xc100; reg SP 0xff00; debug write ioports 0xa8 0xd4')


def persisted(flash, values):
    if not flash.is_file():
        return False
    sectors = formats['decode_flash'](flash)
    data = sectors.get(9)
    return data is not None and all(data[address - 0x20000] == value
                                    for address, value in values.items())


def retry_case(exe, root, firmware):
    out = root/'retry'
    out.mkdir()
    rom = out/'probe.rom'
    rom.write_bytes(image(0x11))
    flash = out/'home/persistent/roms/probe.rom/probe.rom.SRAM.flash'
    emu = Emulator(exe, out, firmware, rom, 'ASCII16-X')
    idle_in_ram(emu)
    backup = flash.with_suffix('.backup')
    try:
        emu.command('set ::save_warnings 0; '
                    'proc count_save_warning {text level} {'
                    'if {$level eq "warning" && [string match "*Couldn*save Flash data*" $text]} '
                    '{incr ::save_warnings}}; set message_callback count_save_warning')
        program(emu, 0x20100, 0x5a)
        wait_for(lambda: persisted(flash, {0x20100: 0x5a}), 'initial timed save')
        good = flash.read_bytes()
        flash.rename(backup)
        # A directory at the destination makes atomic replacement fail on all
        # hosts, without relying on chmod (which behaves differently as root).
        flash.mkdir()
        program(emu, 0x20101, 0x3c)
        wait_for(lambda: int(emu.command('set ::save_warnings')) == 1, 'first failed write')
        assert backup.read_bytes() == good
        # Exercise multiple retries, with no additional guest writes.
        time.sleep(5.5)
        assert int(emu.command('set ::save_warnings')) == 1, 'Repeated warning during one failure'
        assert backup.read_bytes() == good
        assert sorted(p.name for p in flash.parent.iterdir()) == sorted([flash.name, backup.name])
        flash.rmdir()
        backup.rename(flash)
        wait_for(lambda: persisted(flash, {0x20100: 0x5a, 0x20101: 0x3c}),
                 'retry persisted new data without a guest write or shutdown')
        assert emu.process.poll() is None
        assert read(emu, 0x20101) == 0x3c
        saved = flash.read_bytes()
        timestamp = flash.stat().st_mtime_ns
        time.sleep(5.5)
        assert flash.read_bytes() == saved and flash.stat().st_mtime_ns == timestamp, 'Clean save kept retrying'
        # Successful recovery must re-enable warnings for a later failure.
        flash.rename(backup)
        flash.mkdir()
        program(emu, 0x20102, 0x18)
        wait_for(lambda: int(emu.command('set ::save_warnings')) == 2, 'new failure warning')
        flash.rmdir()
        backup.rename(flash)
        wait_for(lambda: persisted(flash, {0x20100: 0x5a, 0x20101: 0x3c, 0x20102: 0x18}),
                 'second recovery')
    finally:
        # Restore the fixture destination even if an assertion failed. The
        # emulator may then perform its normal final save during destruction.
        if flash.is_dir():
            flash.rmdir()
        if backup.exists() and not flash.exists():
            backup.rename(flash)
        emu.close()
    print('PASS timed retry, old-save preservation, warning suppression/reset and clean-save quiescence', flush=True)


def state_flags(emu, path):
    emu.command('store_machine [machine] ' + tcl_path(path))
    root = ET.fromstring(gzip.decompress(path.read_bytes()))
    flash = next(root.iter('flash'))
    return [int(e.text) for e in flash.find('modifiedSectors')]


def reverse_case(exe, root, firmware):
    out = root/'reverse'
    out.mkdir()
    rom = out/'probe.rom'
    rom.write_bytes(image(0x11))
    replay = out/'flash.omr'
    emu = Emulator(exe, out, firmware, rom, 'ASCII16-X')
    idle_in_ram(emu)
    try:
        emu.command('set throttle on; set speed 100; reverse start')
        emu.advance(0.1)
        before = float(emu.command('machine_info time'))
        emu.advance(0.1)
        program(emu, 0x20100, 0x5a)
        emu.advance(0.1)
        after = float(emu.command('machine_info time'))
        emu.command('set pause on')
        assert read(emu, 0x20100) == 0x5a
        assert sum(state_flags(emu, out/'written.oms')) == 1
        emu.command('reverse savereplay ' + tcl_path(replay))
        emu.command(f'reverse goto {before}')
        assert read(emu, 0x20100) == 0xff
        assert sum(state_flags(emu, out/'before.oms')) == 0
        emu.command(f'reverse goto {after}')
        assert read(emu, 0x20100) == 0x5a
        assert sum(state_flags(emu, out/'forward.oms')) == 1
        emu.command('reverse loadreplay -goto end ' + tcl_path(replay))
        assert read(emu, 0x20100) == 0x5a
        assert sum(state_flags(emu, out/'replayed.oms')) == 1
        emu.command('reverse loadreplay -goto begin ' + tcl_path(replay))
        assert read(emu, 0x20100) == 0xff
        assert sum(state_flags(emu, out/'replay-begin.oms')) == 0
    finally:
        emu.close()
    flash = out/'home/persistent/roms/probe.rom/probe.rom.SRAM.flash'
    assert formats['decode_flash'](flash) == {}, 'Rewound empty mask was not persisted'
    print('PASS reverse backward/forward, replay beginning/end and persisted sector history', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--openmsx', type=helpers['executable_path'], required=True)
    parser.add_argument('--only', choices=('retry', 'reverse'))
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix='flash-lifecycle-', dir=ROOT/'derived'))
    print('Artifacts:', out, flush=True)
    firmware = helpers['synthetic_firmware'](out/'synthetic-firmware')
    cases = {'retry': retry_case, 'reverse': reverse_case}
    passed = []
    for name, case in cases.items():
        if args.only and args.only != name:
            continue
        case(args.openmsx, out, firmware)
        passed.append(name)
    (out/'results.json').write_text(json.dumps({'passed': passed}, indent=2))


if __name__ == '__main__':
    main()
