#!/usr/bin/env python3
"""Shared helpers for isolated emulator regression tests."""
import base64
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def image(marker, size=0x40000):
    data = bytearray([255]) * size
    data[:16] = b'AB\x20\x40' + bytes(12)
    data[16:24] = b'ASCII16X'
    # DI; repeatedly read a cartridge byte and copy it into RAM.
    data[32:42] = bytes.fromhex('f3 3a 00 50 32 00 c0 c3 21 40')
    data[0x1000] = marker
    return bytes(data)


def executable_path(value):
    """Accept an explicit native openMSX executable, never a shell/batch command."""
    path = Path(value).expanduser().resolve(strict=True)
    pattern = r'openmsx(?:[-_.][A-Za-z0-9_.-]+)?'
    if os.name == 'nt':
        pattern += r'\.exe'
    if not path.is_file() or not re.fullmatch(pattern, path.name, re.IGNORECASE):
        raise ValueError('Expected a native openMSX executable file')
    if path.suffix.lower() in ('.cmd', '.bat', '.ps1', '.sh', '.py'):
        raise ValueError('Expected a native executable, not a shell wrapper')
    with path.open('rb') as stream:
        magic = stream.read(4)
    if os.name == 'nt':
        native = magic[:2] == b'MZ'
    elif sys.platform == 'darwin':
        native = magic in (bytes.fromhex('feedface'), bytes.fromhex('cefaedfe'),
                           bytes.fromhex('feedfacf'), bytes.fromhex('cffaedfe'),
                           bytes.fromhex('cafebabe'), bytes.fromhex('bebafeca'),
                           bytes.fromhex('cafebabf'), bytes.fromhex('bfbafeca'))
    else:
        native = magic == b'\x7fELF'
    if not native or (os.name != 'nt' and not os.access(path, os.X_OK)):
        raise ValueError('Expected a native executable for this host')
    return path


def tcl_path(path):
    # A command substitution that returns exactly one value, even for braces,
    # dollar signs, semicolons, brackets or newlines in a filesystem path.
    encoded = base64.b64encode(path.as_posix().encode('utf-8')).decode('ascii')
    return '[encoding convertfrom utf-8 [binary decode base64 {' + encoded + '}]]'


class Emulator:
    def __init__(self, exe, out, firmware, rom, mapper, ips=None):
        exe = executable_path(exe)
        if mapper not in ('ASCII16', 'ASCII16-X'):
            raise ValueError('Unsupported synthetic-test mapper')
        out = out.resolve(strict=True)
        rom = rom.resolve(strict=True)
        firmware = firmware.resolve(strict=True)
        if ips is not None:
            ips = ips.resolve(strict=True)
        self.out = out
        self.request = out / 'request.tcl'
        self.response = out / 'response.txt'
        self.log = []
        user = out / 'home/share'
        (user / 'systemroms').mkdir(parents=True)
        for name in ('nms8250_basic-bios2.rom', 'nms8250_msx2sub.rom', 'nms8250_disk.rom'):
            shutil.copy2(firmware / name, user / 'systemroms' / name)
        script = out / 'start.tcl'
        script.write_text("""set renderer none
set throttle off
set save_settings_on_exit false
set mute on
proc poll_test {} {
    set request [file join $::env(ROM_TEST_DIR) request.tcl]
    if {[file exists $request]} {
        set f [open $request r];set script [read $f];close $f
        file delete $request
        set status [catch {uplevel #0 $script} value]
        set f [open [file join $::env(ROM_TEST_DIR) response.tmp] w]
        puts $f $status
        puts $f [binary encode base64 -maxlen 0 [encoding convertto utf-8 $value]]
        close $f
        file rename [file join $::env(ROM_TEST_DIR) response.tmp] [file join $::env(ROM_TEST_DIR) response.txt]
    }
    after realtime 0.01 poll_test
}
after realtime 0.01 poll_test
""")
        env = dict(os.environ, OPENMSX_HOME=str(out / 'home'), OPENMSX_USER_DATA=str(user),
                   OPENMSX_SYSTEM_DATA=str(ROOT / 'share'), ROM_TEST_DIR=str(out),
                   SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        args = [str(exe), '-machine', test_machine(user, firmware), '-cart', str(rom), '-romtype', mapper]
        if ips:
            args += ['-ips', str(ips)]
        args += ['-script', str(script)]
        self.stderr = (out / 'stderr.txt').open('w')
        self.process = subprocess.Popen(args, shell=False, env=env, stdout=self.stderr, stderr=self.stderr,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        try:
            self.command('set power on; set pause off')
            self.advance(3)
        except BaseException:
            self.process.kill(); self.process.wait(); self.stderr.close()
            (self.out / 'commands.json').write_text(json.dumps(self.log, indent=2))
            raise

    def command(self, script):
        temporary = self.out / 'request.tmp'
        temporary.write_text(script, encoding='utf-8')
        temporary.rename(self.request)
        until = time.monotonic() + 20
        while not self.response.exists():
            assert self.process.poll() is None, 'Emulator exited: ' + (self.out / 'stderr.txt').read_text()
            assert time.monotonic() < until, 'Command timed out: ' + script
            time.sleep(0.01)
        while True:
            try:
                response = self.response.read_text()
                self.response.unlink()
                break
            except PermissionError:
                assert time.monotonic() < until, 'Response file remained locked'
                time.sleep(0.01)
        status, value = response.splitlines()
        value = base64.b64decode(value).decode('utf-8')
        self.log.append((script, status, value))
        assert status == '0', (script, value)
        return value

    def advance(self, seconds=0.02):
        self.command(f'set ::test_tick 0; after time {seconds} {{set ::test_tick 1}}')
        until = time.monotonic() + 20
        while self.command('set ::test_tick') != '1':
            assert time.monotonic() < until, 'Emulation did not advance'
            time.sleep(0.005)

    def expect(self, marker, raw, patched=None):
        self.advance()
        assert int(self.command('debug read memory 0xc000')) == marker, 'Running ROM changed or stopped'
        for key, data in [('originalSHA1', raw), ('actualSHA1', patched or raw)]:
            # Match openMSX's SHA-1 ROM identity, not a security/authentication check.
            actual = self.command(f'dict get [machine_info device [guess_rom_device]] {key}')
            assert actual == hashlib.sha1(data, usedforsecurity=False).hexdigest(), (key, actual)

    def close(self):
        try:
            if self.process.poll() is None:
                self.request.write_text('exit')
                self.process.wait(timeout=10)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()
            self.stderr.close()
            (self.out / 'commands.json').write_text(json.dumps(self.log, indent=2), encoding='utf-8')



def fixture_path(root, path):
    """Reject writes outside the fresh fixture directory, including symlinks."""
    # Keep the lexical root as the walk boundary: macOS /var itself is an
    # alias for /private/var, outside the owned fixture directory.
    root = Path(root).absolute()
    path = Path(path).absolute()
    resolved_root = root.resolve(strict=True)
    resolved = path.resolve()
    if resolved == resolved_root or not resolved.is_relative_to(resolved_root):
        raise ValueError('Fixture path escapes the isolated test directory')
    # Reject even in-root symlink aliases: each fixture has one owned pathname.
    for entry in (path, *path.parents):
        if entry == root:
            break
        if entry.is_symlink() or (hasattr(entry, 'is_junction') and entry.is_junction()):
            raise ValueError('Fixture paths must not contain symlinks or junctions')
    return resolved


def write_fixture(root, path, data):
    """Write only an explicitly validated file under a disposable profile."""
    destination = fixture_path(root, path)
    with destination.open('wb') as stream:
        stream.write(data)


def synthetic_firmware(directory):
    """Minimal original boot code, not an MSX BIOS or a commercial ROM.

    Configure the 8250 PPI, RAM subslot and mapper, then enter the synthetic
    cartridge at 4020h. Persistence tests need no BIOS services or interrupts.
    """
    directory.mkdir(parents=True)
    boot = bytes.fromhex(
        'f3 3e 82 d3 ab 3e c0 d3 a8 3e aa 32 ff ff '
        '3e 03 d3 fc 3e 02 d3 fd 3e 01 d3 fe af d3 ff '
        '3e d4 d3 a8 31 00 f0 c3 20 40')
    (directory/'nms8250_basic-bios2.rom').write_bytes(boot.ljust(0x8000, b'\xff'))
    (directory/'nms8250_msx2sub.rom').write_bytes(b'\xff' * 0x4000)
    (directory/'nms8250_disk.rom').write_bytes(b'\xff' * 0x4000)
    (directory/'synthetic-boot.txt').write_text('Original minimal boot code for Flash tests.\n')
    return directory


def test_machine(user, firmware):
    if not (firmware/'synthetic-boot.txt').exists():
        return 'Philips_NMS_8250'
    import xml.etree.ElementTree as ET
    config = ET.parse(ROOT/'share/machines/Philips_NMS_8250.xml')
    for rom in config.iter('rom'):
        for checksum in list(rom.findall('sha1')):
            rom.remove(checksum)
        filename = rom.find('filename')
        filename.text = (user/'systemroms'/filename.text).resolve().as_posix()
    description = config.find('info/description')
    description.text = 'Synthetic Flash test machine; no BIOS services'
    machines = user/'machines'
    machines.mkdir(exist_ok=True)
    (machines/'Flash_Test.xml').write_bytes(
        b'<?xml version="1.0"?>\n<!DOCTYPE msxconfig SYSTEM "msxconfig2.dtd">\n' +
        ET.tostring(config.getroot()))
    return 'Flash_Test'
