#!/usr/bin/env python3
"""Test complete Flash fallback and explicit flashname using a second chip.

Uses a synthetic MFR SCC+ SD extension without SD cards or commercial ROMs.
"""
import argparse
import base64
import gzip
import json
from pathlib import Path
import runpy
import tempfile
import xml.etree.ElementTree as ET
import zlib

ROOT = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(ROOT / 'Contrib/rom-test-utils.py'))
formats = runpy.run_path(str(ROOT / 'Contrib/flash-persistence-test.py'))
Emulator, tcl_path, image = (helpers[n] for n in ('Emulator', 'tcl_path', 'image'))


def check_protected_sectors(emu, out):
    factory = out / 'protected.rom'
    factory.write_bytes(image(0x42, 0x80000))
    flash = out / 'protected.flash'
    content = bytearray(image(0x31, 0x80000))
    content[0x70100] = 0x5a
    flags = [0] * 7 + [1]
    config = ET.Element('msxconfig')
    primary = ET.SubElement(ET.SubElement(config, 'devices'), 'primary', slot='any')
    device = ET.SubElement(ET.SubElement(primary, 'secondary', slot='any'), 'ROM', id='Protected fixture')
    ET.SubElement(device, 'mem', base='0x0000', size='0x10000')
    ET.SubElement(ET.SubElement(device, 'sound'), 'volume').text = '9000'
    ET.SubElement(ET.SubElement(device, 'rom'), 'filename').text = factory.as_posix()
    ET.SubElement(device, 'mappertype').text = 'Manbow2'
    ET.SubElement(device, 'flashname').text = flash.as_posix()
    extension = out / 'home/share/extensions/protected-fixture.xml'
    extension.write_bytes(b'<?xml version="1.0"?>\n<!DOCTYPE msxconfig SYSTEM "msxconfig2.dtd">\n' + ET.tostring(config))
    bad_flags = [1] + flags[1:]
    bad = formats['encode_document'](formats['flash_document'](content, bad_flags, 'AM29F040B', [0x10000] * 8))
    flash.write_bytes(bad)
    error = emu.command('catch {ext ' + extension.stem + '} reason; set reason')
    assert 'read-only sector' in error, error
    assert flash.read_bytes() == bad
    flash.write_bytes(formats['encode_document'](formats['flash_document'](content, flags, 'AM29F040B', [0x10000] * 8)))
    emu.command('set fixture [ext ' + extension.stem + ']')
    debuggable = emu.command('lsearch -inline -glob [debug list] *_flash')
    assert debuggable
    # This mapper's SRAM contains only the last 64 KiB writable sector.
    assert emu.command('debug read {' + debuggable + '} 0x100') == '90'
    state = out / 'protected.oms'
    emu.command('store_machine [machine] ' + tcl_path(state))
    emu.command('set old [machine]; set restored [restore_machine ' + tcl_path(state) + ']; delete_machine $old; activate_machine $restored')
    emu.command('remove_extension $fixture')
    saved = ET.fromstring(gzip.decompress(flash.read_bytes())).find('flash')
    data = zlib.decompress(base64.b64decode(saved.findtext('content')))
    assert len(data) == 0x80000 and data[0x1000] == 0x42 and data[0x70100] == 0x5a
    assert [int(e.text) for e in saved.find('modifiedSectors')] == flags


def check_sdmapper_v1(emu, out):
    # This chip was added upstream after the first persistence revision. Its
    # identity must be populated even when Release builds disable assertions.
    factory = out/'sdmapper.rom'
    factory.write_bytes(image(0x42, 0x20000))
    flash = out/'sdmapper.flash'
    content = bytearray(image(0x31, 0x20000))
    content[0x10100] = 0x5a
    flags = [0, 0, 0, 0, 1, 0, 0, 0]
    flash.write_bytes(formats['encode_document'](
        formats['flash_document'](content, flags, 'AM29F010', [0x4000] * 8)))
    config = ET.Element('msxconfig')
    primary = ET.SubElement(ET.SubElement(config, 'devices'), 'primary', slot='any')
    device = ET.SubElement(ET.SubElement(primary, 'secondary', slot='any'),
                           'SdMapperV1', id='SD fixture')
    ET.SubElement(device, 'mem', base='0x0000', size='0x10000')
    ET.SubElement(ET.SubElement(device, 'rom'), 'filename').text = factory.as_posix()
    ET.SubElement(device, 'flashname').text = flash.as_posix()
    ET.SubElement(device, 'ram').text = 'false'
    # No SD card children: no real or synthetic disk images are opened.
    extension = out/'home/share/extensions/sd-fixture.xml'
    extension.write_bytes(b'<?xml version="1.0"?>\n<!DOCTYPE msxconfig SYSTEM "msxconfig2.dtd">\n' +
                          ET.tostring(config))
    emu.command('set fixture [ext ' + extension.stem + ']')
    assert emu.command('debug read {SD fixture flash} 0x1000') == '66'
    assert emu.command('debug read {SD fixture flash} 0x10100') == '90'
    state = out/'sdmapper.oms'
    emu.command('store_machine [machine] ' + tcl_path(state))
    emu.command('set old [machine]; set restored [restore_machine ' + tcl_path(state) +
                ']; delete_machine $old; activate_machine $restored')
    emu.command('remove_extension $fixture')
    saved = ET.fromstring(gzip.decompress(flash.read_bytes())).find('flash')
    assert saved.findtext('chipType') == 'AM29F010'
    assert [int(e.text) for e in saved.find('modifiedSectors')] == flags
    data = zlib.decompress(base64.b64decode(saved.findtext('content')))
    assert len(data) == 0x20000 and data[0x1000] == 0x42 and data[0x10100] == 0x5a


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--openmsx', type=helpers['executable_path'], required=True)
    firmware_source = parser.add_mutually_exclusive_group(required=True)
    firmware_source.add_argument('--firmware-dir', type=Path)
    firmware_source.add_argument('--synthetic-firmware', action='store_true')
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix='flash-device-', dir=ROOT / 'derived'))
    firmware = helpers['synthetic_firmware'](out/'synthetic-firmware') if args.synthetic_firmware else args.firmware_dir
    rom = out / 'bootstrap.rom'
    rom.write_bytes(image(0x11))
    flash = out / 'explicit.flash'
    content = bytearray(image(0x31)).ljust(0x800000, b'\xff')
    content[0x20100] = 0x5a
    flags = [0] * len(formats['SIZES'])
    flags[9] = 1
    flash.write_bytes(formats['encode_document'](formats['flash_document'](content, flags, 'M29W640GB')))

    config = ET.Element('msxconfig')
    devices = ET.SubElement(config, 'devices')
    primary = ET.SubElement(devices, 'primary', slot='any')
    secondary = ET.SubElement(primary, 'secondary', slot='any')
    device = ET.SubElement(secondary, 'MegaFlashRomSCCPlusSD', id='Flash fixture')
    ET.SubElement(device, 'mem', base='0x0000', size='0x10000')
    ET.SubElement(ET.SubElement(device, 'sound'), 'volume').text = '9000'
    rom_tag = ET.SubElement(device, 'rom')
    # No sramname: flashname is an independent supported configuration option.
    ET.SubElement(device, 'flashname').text = flash.as_posix()
    ET.SubElement(device, 'hasmemorymapper').text = 'false'
    extension = out / 'home/share/extensions/flash-fixture.xml'
    extension.parent.mkdir(parents=True)

    def save_config():
        extension.write_bytes(b'<?xml version="1.0"?>\n<!DOCTYPE msxconfig SYSTEM "msxconfig2.dtd">\n' + ET.tostring(config))

    save_config()
    emu = Emulator(args.openmsx, out, firmware, rom, 'ASCII16')
    reports = []
    try:
        emu.command('set power off; carta eject')
        emu.command('set fixture [ext ' + extension.stem + ']')
        assert emu.command('debug read {MFR SCC+ SD flash} 0x1000') == '49'
        assert emu.command('debug read {MFR SCC+ SD flash} 0x20100') == '90'
        reports.append('Complete image retains unmodified contents without an external ROM')

        state = out / 'without-rom.oms'
        emu.command('store_machine [machine] ' + tcl_path(state))
        emu.command('set old [machine]; set restored [restore_machine ' + tcl_path(state) + ']; delete_machine $old; activate_machine $restored')
        assert emu.command('debug read {MFR SCC+ SD flash} 0x1000') == '49'
        emu.command('remove_extension $fixture')
        saved = ET.fromstring(gzip.decompress(flash.read_bytes())).find('flash')
        assert saved.findtext('chipType') == 'M29W640GB'
        assert [int(e.text) for e in saved.find('modifiedSectors')] == flags
        data = zlib.decompress(base64.b64decode(saved.findtext('content')))
        assert data == content
        reports.append('Explicit flashname works without sramname; state restore persists the full chip')

        factory = out / 'factory.rom'
        factory.write_bytes(image(0x42))
        ET.SubElement(rom_tag, 'filename').text = factory.as_posix()
        save_config()
        emu.command('set fixture [ext ' + extension.stem + ']')
        assert emu.command('debug read {MFR SCC+ SD flash} 0x1000') == '66'
        assert emu.command('debug read {MFR SCC+ SD flash} 0x20100') == '90'
        emu.command('remove_extension $fixture')
        reports.append('A supplied factory ROM refreshes untouched sectors while preserving saved sectors')
        check_protected_sectors(emu, out)
        reports.append('Protected sectors use current ROM bytes; complete persistence retains writable data and rejects protected-sector flags')
        check_sdmapper_v1(emu, out)
        reports.append('SD Mapper v1 persists AM29F010 identity, geometry and modification history')
    finally:
        emu.close()
    (out / 'results.json').write_text(json.dumps(reports, indent=2))
    for report in reports:
        print('PASS', report, flush=True)
    print('Artifacts:', out)


if __name__ == '__main__':
    main()
