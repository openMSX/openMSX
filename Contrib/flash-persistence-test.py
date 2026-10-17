#!/usr/bin/env python3
"""End-to-end Flash persistence checks with synthetic ROMs and isolated profiles.

Run from any directory. Requires Python 3 and locally available Philips NMS8250
firmware; no commercial game is used. Files are retained in a new derived folder.
"""
import argparse
import base64
import gzip
import json
import os
import runpy
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import tempfile
import zlib
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MAGIC = b'OMSXFLS\x01'
SIZES = [0x2000] * 8 + [0x10000] * 127
helpers = runpy.run_path(str(ROOT / 'Contrib/rom-replacement-test.py'))
executable_path, tcl_path = (helpers[n] for n in ('executable_path', 'tcl_path'))
PENDING_STATE = 'pending.oms'
MARKER, SAVE, SECOND = 0x1000, 0x20100, 0x30100


def image(version):
    data = bytearray([255]) * 0x40000
    data[:4] = b'AB\x20\x40'
    data[4:16] = bytes(12)
    data[16:24] = b'ASCII16X'
    data[32:36] = b'\xf3\xc3\x21\x40'
    data[MARKER] = version
    return data


def flash_document(content, flags=None, chip='S29GL064S70TFI040', sizes=SIZES):
    """Independent fixture writer using openMSX's documented archive fields."""
    root = ET.Element('serial')
    flash = ET.SubElement(root, 'flash', version='1')
    ET.SubElement(flash, 'chipType').text = chip
    for name, values in [('sectorSizes', sizes),
                         ('modifiedSectors', flags if flags is not None else [0] * len(sizes))]:
        field = ET.SubElement(flash, name)
        for value in values:
            ET.SubElement(field, 'item').text = str(value)
    ET.SubElement(flash, 'content', encoding='gz-base64').text = base64.b64encode(zlib.compress(content)).decode()
    return root


def encode_document(root):
    header = b'<?xml version="1.0"?>\n<!DOCTYPE openmsx-serialize SYSTEM "openmsx-serialize.dtd">\n'
    return gzip.compress(header + ET.tostring(root), mtime=0)


def decode_flash(path: Path):
    flash = ET.fromstring(gzip.decompress(path.read_bytes())).find('flash')
    assert flash is not None and flash.get('version', '1') == '1'
    assert flash.findtext('chipType') == 'S29GL064S70TFI040'
    sizes = [int(e.text) for e in flash.find('sectorSizes')]
    flags = [int(e.text) for e in flash.find('modifiedSectors')]
    assert sizes == SIZES and len(flags) == len(SIZES) and set(flags) <= {0, 1}
    assert flash.find('content').get('encoding') == 'gz-base64'
    data = zlib.decompress(base64.b64decode(flash.findtext('content')))
    assert len(data) == 0x800000
    # No command/status/timing state belongs in persistent storage.
    assert {e.tag for e in flash} == {'chipType', 'sectorSizes', 'modifiedSectors', 'content'}
    sectors, offset = {}, 0
    for i, size in enumerate(sizes):
        if flags[i]:
            sectors[i] = data[offset:offset + size]
        offset += size
    return sectors


def sparse_fixture(sectors):
    header = MAGIC + struct.pack('<II', 0x800000, len(SIZES))
    payload = header + b''.join(struct.pack('<IB', size, int(i in sectors)) for i, size in enumerate(SIZES))
    payload += b''.join(sectors[i] for i in sorted(sectors))
    return payload + struct.pack('<I', zlib.crc32(payload))


def check_corrupt_files(flash: Path, run):
    good = flash.read_bytes()
    assert decode_flash(flash) == {}
    content = bytes(image(0x11)).ljust(0x800000, b'\xff')
    fixture = encode_document(flash_document(content))
    flash.write_bytes(fixture)
    run('independent-full-image', [('expect', MARKER, 0x33), ('expect', SAVE, 0x88)])
    # The unmodified marker in the old full image must not hide the current ROM.
    assert flash.read_bytes() == fixture
    variants = [('truncated', fixture[:20])]
    for name, change in [
        ('wrong-chip-same-geometry', lambda f: setattr(f.find('chipType'), 'text', 'M29W640GB')),
        ('bad-sector-size', lambda f: setattr(f.find('sectorSizes/item'), 'text', '4096')),
        ('bad-sector-flag', lambda f: setattr(f.find('modifiedSectors/item'), 'text', '2')),
        ('overflow-sector-flag', lambda f: setattr(f.find('modifiedSectors/item'), 'text', '256')),
        ('wide-overflow-sector-flag', lambda f: setattr(f.find('modifiedSectors/item'), 'text', '4294967296')),
        ('empty-sector-flag', lambda f: setattr(f.find('modifiedSectors/item'), 'text', '')),
        ('missing-sector-flag', lambda f: f.find('modifiedSectors').remove(f.find('modifiedSectors/item'))),
        ('future-format', lambda f: f.set('version', '2')),
        ('missing-payload', lambda f: f.remove(f.find('content'))),
        ('bad-payload', lambda f: setattr(f.find('content'), 'text', base64.b64encode(zlib.compress(b'short')).decode())),
    ]:
        document = flash_document(content)
        change(document.find('flash'))
        variants.append((name, encode_document(document)))
    # An older valid save must never conceal a corrupt/incompatible new one.
    old_sparse = Path(str(flash).removesuffix('.flash') + '.sparse')
    old_sparse.write_bytes(sparse_fixture({}))
    for name, bad in variants:
        flash.write_bytes(bad)
        run(name, [], expect_error=True)
        assert flash.read_bytes() == bad
        assert old_sparse.read_bytes() == sparse_fixture({})
    old_sparse.unlink()
    with flash.open('wb') as restored:
        restored.write(good)
    run('valid-after-corrupt', [('expect', MARKER, 0x33)])
    if os.name == 'nt':
        existing = set(flash.parent.iterdir())
        flash.chmod(stat.S_IREAD)
        try:
            run('replace-failure', [('program', SECOND, 0xf0), ('expect', SECOND, 0xf0)])
            assert flash.read_bytes() == good
            assert set(flash.parent.iterdir()) == existing
        finally:
            flash.chmod(stat.S_IWRITE | stat.S_IREAD)
        run('after-replace-failure', [('expect', SECOND, 0xff)])


def check_sparse_migration(flash: Path, run, marker):
    old_sparse = Path(str(flash).removesuffix('.flash') + '.sparse')
    # Erased sectors must retain their modification history, not just written data.
    raw = sparse_fixture({9: b'\xff' * 0x10000})
    flash.unlink()
    bad = raw[:-1] + bytes([raw[-1] ^ 1])
    old_sparse.write_bytes(bad)
    run('sparse-corrupt', [], expect_error=True)
    assert old_sparse.read_bytes() == bad and not flash.exists()
    old_sparse.write_bytes(raw)
    run('sparse-load', [('expect', MARKER, marker), ('expect', SAVE, 0xff)])
    assert not flash.exists() and old_sparse.read_bytes() == raw
    run('sparse-migrate', [('program', SECOND, 0xe0), ('expect', SECOND, 0xe0)])
    assert set(decode_flash(flash)) == {9, 10}
    assert old_sparse.read_bytes() == raw
    run('serialized-precedence', [('expect', SECOND, 0xe0)])
    old_sparse.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--openmsx', type=executable_path, default=ROOT/'derived/x64-VC-Release/install/openmsx.exe')
    parser.add_argument('--firmware-dir', type=Path, required=True)
    parser.add_argument('--baseline', action='store_true', help='Confirm the old whole-image persistence bug')
    parser.add_argument('--ram-routines', type=Path, help='Assembled flash-persistence-ram.asm, loaded at C200h')
    parser.add_argument('--legacy-fixtures', type=Path, help='Artifact directory from a --baseline run')
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix='flash-baseline-' if args.baseline else 'flash-regression-', dir=ROOT/'derived'))
    profile = out/'home'
    user = profile/'share'
    firmware = user/'systemroms'
    firmware.mkdir(parents=True)
    for name in ('nms8250_basic-bios2.rom','nms8250_msx2sub.rom','nms8250_disk.rom'):
        shutil.copy2(args.firmware_dir/name, firmware/name)
    rom = out/'probe.rom'
    persistent = profile/'persistent/roms/probe.rom/probe.rom.SRAM'
    flash = Path(str(persistent)+'.flash')
    count = 0

    def run(name, actions, expect_error=False):
        nonlocal count
        count += 1
        plan = out/f'{count:02d}-{name}.tcl'
        result = out/f'{count:02d}-{name}.txt'
        def atom(value):
            return tcl_path(Path(str(value).replace('\\', '/')))
        plan.write_text('set ::actions [list ' + ' '.join('[list '+' '.join(map(atom,a))+']' for a in actions) + ']\n')
        env = dict(os.environ, OPENMSX_HOME=str(profile), OPENMSX_USER_DATA=str(user),
                   OPENMSX_SYSTEM_DATA=str(ROOT/'share'), SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                   FLASH_PLAN=str(plan), FLASH_RESULT=str(result),
                   FLASH_RAM_ROUTINES=str(args.ram_routines.resolve()) if args.ram_routines else '')
        process = subprocess.run([str(args.openmsx.resolve()), '-machine','Philips_NMS_8250',
                    '-cart',str(rom),'-romtype','ASCII16-X','-script',str(Path(__file__).with_suffix('.tcl'))],
                    shell=False, env=env, capture_output=True, text=True, timeout=60,
                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        (out/f'{count:02d}-{name}.json').write_text(json.dumps({'returncode': process.returncode, 'stdout': process.stdout, 'stderr': process.stderr},indent=2))
        if expect_error:
            assert process.returncode == 1 and not result.exists(), (name,process.returncode,process.stdout,process.stderr)
        else:
            assert process.returncode == 0 and result.exists() and result.read_text().strip() == 'OK', (name, process.returncode, process.stdout, process.stderr, result.read_text() if result.exists() else 'no result')
        print('PASS',name,flush=True)

    rom.write_bytes(image(0x11))
    before = out/'before.oms'
    written = out/'written.oms'
    run('clean', [('expect',MARKER,0x11), ('save',before)])
    assert not persistent.exists() and not flash.exists()
    run('program', [('program',SAVE,0x5a),('expect',SAVE,0x5a),('save',written)])
    rom.write_bytes(image(0x22))
    run('updated-rom', [('expect',MARKER,0x11 if args.baseline else 0x22),('expect',SAVE,0x5a)])
    if args.baseline:
        assert persistent.stat().st_size == 0x800000
        print('Confirmed: updated ROM marker is hidden by the persisted image.')
        print('Artifacts:',out)
        return
    assert set(decode_flash(flash)) == {9}
    assert not persistent.exists()
    run('second-sector', [('program',SECOND,0xf0),('expect',SECOND,0xf0)])
    assert set(decode_flash(flash)) == {9,10}
    run('restore-written', [('load',written),('expect',SAVE,0x5a),('expect',SECOND,0xff)])
    assert set(decode_flash(flash)) == {9}
    run('restart-written', [('expect',MARKER,0x22),('expect',SAVE,0x5a),('expect',SECOND,0xff)])
    run('erase', [('erase',SAVE),('expect',SAVE,0xff)])
    assert set(decode_flash(flash)) == {9}
    v3=image(0x33);v3[SAVE]=0x88;rom.write_bytes(v3)
    run('erased-stays-erased', [('expect',MARKER,0x33),('expect',SAVE,0xff)])
    run('restore-clean', [('load',before),('expect',SAVE,0xff)])
    assert decode_flash(flash) == {}
    run('restart-clean', [('expect',MARKER,0x33),('expect',SAVE,0x88)])
    check_corrupt_files(flash, run)
    if args.ram_routines:
        # CPU executes the supplied routines from RAM, including status polling.
        # Map commands in page 1, data in page 2, code/source/stack in page 3.
        run('ram-program', [('ram-program',), ('ram-result',0), ('ram-data',),
                            ('expect',MARKER,0x33)])
        assert set(decode_flash(flash)) == {9}
        v4=image(0x44);rom.write_bytes(v4)
        run('ram-restart', [('expect',MARKER,0x44), ('ram-data',)])
        run('ram-erase', [('ram-erase',), ('ram-result',0), ('ram-erased',)])
        assert decode_flash(flash)[9] == bytes([255])*0x10000
        run('ram-erase-restart', [('ram-erased',), ('expect',MARKER,0x44)])
        run('ram-pending-state', [('ram-program-pending',), ('save',out/PENDING_STATE),
                                 ('wait',1), ('ram-result',0),
                                 ('load',out/PENDING_STATE), ('wait',1),
                                 ('ram-result',0), ('ram-data',)])
        state=ET.fromstring(gzip.decompress((out/PENDING_STATE).read_bytes()))
        assert any(f.findtext('state') == 'PROGRAM' for f in state.iter('flash'))
        # These two status inputs are deliberately synthetic RAM fixtures,
        # not claims that a real Flash timing-limit failure was induced.
        run('ram-dq5-failure', [('ram-status',0xa0,0), ('ram-result',1),
                                ('ram-status-byte',0xf0)])
        run('ram-dq7-success', [('ram-status',0x20,0x20), ('ram-result',0),
                                ('ram-status-byte',0x20)])
    check_sparse_migration(flash, run, 0x44 if args.ram_routines else 0x33)
    if args.legacy_fixtures:
        legacy=args.legacy_fixtures.resolve()
        flash.unlink()
        raw=(legacy/'home/persistent/roms/probe.rom/probe.rom.SRAM').read_bytes()
        persistent.write_bytes(raw)
        run('legacy-load', [('expect',MARKER,0x11), ('expect',SAVE,0x5a)])
        assert not flash.exists() and persistent.read_bytes() == raw
        run('legacy-migrate', [('program',SECOND,0xe0), ('expect',SECOND,0xe0)])
        assert set(decode_flash(flash)) == set(range(len(SIZES)))
        assert persistent.read_bytes() == raw
        run('legacy-v4-state', [('load',legacy/'written.oms'), ('expect',SAVE,0x5a),
                                ('expect',SECOND,0xff)])
        assert set(decode_flash(flash)) == set(range(len(SIZES)))
        run('legacy-v4-restart', [('expect',SAVE,0x5a), ('expect',SECOND,0xff)])
    print('Artifacts:',out)
    print(f'{count} emulator sessions passed.')

if __name__ == '__main__':
    main()
