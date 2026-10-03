#!/usr/bin/env python3
"""Exercise host-side ROM replacement while openMSX holds an inserted cartridge.

Windows regression for issue #955; uses synthetic ROMs and disposable profiles.
No user ROM or save file is changed. Retains test artifacts under derived/.
"""
import argparse
import base64
import gzip
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
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
    if os.name == 'nt':
        with path.open('rb') as stream:
            if stream.read(2) != b'MZ':
                raise ValueError('Expected a Windows executable, not a command script')
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
        args = [str(exe), '-machine', 'Philips_NMS_8250', '-cart', str(rom), '-romtype', mapper]
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


def run_case(exe, out, firmware, mapper, compressed=False, patch=False, baseline=False):
    out.mkdir()
    rom = out / ('development.rom.gz' if compressed else 'development.rom')
    raw = image(0x11)
    encode = (lambda b: gzip.compress(b, mtime=0)) if compressed else (lambda b: b)
    rom.write_bytes(encode(raw))
    patched = bytearray(raw)
    ips = None
    marker = 0x11
    if patch:
        ips = out / 'development.ips'
        # One IPS byte, same file size. The original SHA1 must stay unpatched.
        ips.write_bytes(b'PATCH' + b'\x00\x10\x00\x00\x01\x33' + b'EOF')
        patched[0x1000] = marker = 0x33
    emulator = Emulator(exe, out, firmware, rom, mapper, ips)
    events = []
    try:
        emulator.expect(marker, raw, bytes(patched))
        if baseline:
            try:
                rom.write_bytes(encode(image(0x22)))
            except OSError as error:
                events.append(f'Expected baseline failure: {error}')
            else:
                raise AssertionError('Baseline unexpectedly allowed truncating the loaded ROM')
            return events

        # An ordinary save-state roundtrip must retain the image and ROM hashes.
        state = out / 'before.oms'
        emulator.command('store_machine [machine] ' + tcl_path(state))
        emulator.command('set old [machine]; set new [restore_machine ' + tcl_path(state) + ']; delete_machine $old; activate_machine $new')
        emulator.expect(marker, raw, bytes(patched))
        events.append('save-state restore and original/patched hashes')

        # Existing build tools use different write/replace strategies.
        for operation in ('overwrite', 'truncate', 'replace', 'rename', 'delete'):
            fresh = image(0x22)
            if operation == 'overwrite':
                with rom.open('r+b') as stream:
                    stream.write(encode(fresh))
            elif operation == 'truncate':
                rom.write_bytes(encode(image(0x22, 0x8000)))
            elif operation == 'replace':
                temporary = out / 'replacement.tmp'
                temporary.write_bytes(encode(fresh))
                os.replace(temporary, rom)
            elif operation == 'rename':
                renamed = out / 'renamed.tmp'
                rom.rename(renamed)
                renamed.rename(rom)
            else:
                rom.unlink()
                emulator.expect(marker, raw, bytes(patched))
                rom.write_bytes(encode(fresh))
            emulator.expect(marker, raw, bytes(patched))
            events.append(operation + ': host write succeeds; running snapshot and hashes unchanged')

        # The existing SHA1 file pool caches whole-second mtimes. Ensure the
        # replacement is distinguishable even when this test completes rapidly.
        stamp = rom.stat()
        os.utime(rom, (stamp.st_atime, stamp.st_mtime + 2))

        # Insert the new file without the IPS; no emulator restart required.
        emulator.command('carta eject')
        emulator.command('carta ' + tcl_path(rom) + ' -romtype ' + mapper)
        emulator.command('reset')
        emulator.advance(3)
        emulator.expect(0x22, image(0x22))
        events.append('reinsertion loads the rebuilt image and new hashes')
        return events
    finally:
        emulator.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--openmsx', type=executable_path, default=ROOT / 'derived/x64-VC-Release/install/openmsx.exe')
    parser.add_argument('--firmware-dir', type=Path, required=True)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix='rom-replacement-', dir=ROOT / 'derived'))
    reports = {}
    cases = [('ascii16', 'ASCII16', False, False), ('asciix16', 'ASCII16-X', False, False)]
    if not args.baseline:
        cases += [('gzip', 'ASCII16', True, False), ('ips', 'ASCII16', False, True)]
    for name, mapper, compressed, patch in cases:
        reports[name] = run_case(args.openmsx, out / name, args.firmware_dir, mapper, compressed, patch, args.baseline)
        print('PASS', name, *reports[name], sep='\n  ', flush=True)
    (out / 'results.json').write_text(json.dumps(reports, indent=2) + '\n')
    print('Artifacts:', out)


if __name__ == '__main__':
    main()
