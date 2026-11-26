#!/usr/bin/env python3
"""Validate native-executable and Tcl-path boundaries in the ROM test harness."""
import base64
import os
from pathlib import Path
import runpy
import tempfile
import unittest

helpers = runpy.run_path(str(Path(__file__).with_name('rom-test-utils.py')))
executable_path = helpers['executable_path']
tcl_path = helpers['tcl_path']
write_fixture = helpers['write_fixture']


class HarnessValidation(unittest.TestCase):
    def test_option_and_command_text_are_not_executables(self):
        for value in ('-script', 'openmsx --script injected.tcl', 'openmsx; whoami'):
            with self.subTest(value=value), self.assertRaises((OSError, ValueError)):
                executable_path(value)

    def test_rejects_shell_wrapper_and_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(ValueError):
                executable_path(root)
            batch = root / 'openmsx.cmd'
            batch.write_text('@echo not an emulator')
            with self.assertRaises(ValueError):
                executable_path(batch)
            shell = root / ('openmsx-test.exe' if os.name == 'nt' else 'openmsx')
            shell.write_text('#!/bin/sh\nexit 0\n')
            shell.chmod(0o755)
            with self.assertRaises(ValueError):
                executable_path(shell)
            if os.name == 'nt':
                fake_exe = root / 'openmsx.exe'
                fake_exe.write_text('@echo not a PE executable')
                with self.assertRaises(ValueError):
                    executable_path(fake_exe)

    def test_fixture_writes_reject_escape_and_aliases(self):
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory)
            root = parent/'owned'
            root.mkdir()
            outside = parent/'outside.bin'
            outside.write_bytes(b'untouched')
            with self.assertRaises(ValueError):
                write_fixture(root, root/'../outside.bin', b'bad')
            with self.assertRaises(ValueError):
                write_fixture(root, outside, b'bad')
            write_fixture(root, root/'good.bin', b'fixture')
            self.assertEqual((root/'good.bin').read_bytes(), b'fixture')
            self.assertEqual(outside.read_bytes(), b'untouched')

    def test_fixture_writes_reject_symlinks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root/'target.bin'
            target.write_bytes(b'untouched')
            link = root/'alias.bin'
            try:
                link.symlink_to(target)
            except OSError as error:
                self.skipTest(f'Symlink creation unavailable: {error}')
            with self.assertRaises(ValueError):
                write_fixture(root, link, b'bad')
            self.assertEqual(target.read_bytes(), b'untouched')
            # A system alias above our owned root (e.g. macOS /var) is valid.
            actual = root/'actual'
            actual.mkdir()
            parent_alias = root/'parent-alias'
            parent_alias.symlink_to(actual, target_is_directory=True)
            owned = parent_alias/'owned'
            owned.mkdir()
            write_fixture(owned, owned/'good.bin', b'fixture')
            self.assertEqual((actual/'owned/good.bin').read_bytes(), b'fixture')

    def test_tcl_path_contains_only_encoded_user_bytes(self):
        original = Path('assets/} ; set injected 1; # [exit] $name ' + chr(0x65e5) + '.rom')
        word = tcl_path(original)
        prefix = '[encoding convertfrom utf-8 [binary decode base64 {'
        self.assertTrue(word.startswith(prefix))
        self.assertTrue(word.endswith('}]]'))
        encoded = word[len(prefix):-3]
        self.assertEqual(base64.b64decode(encoded, validate=True).decode('utf-8'), original.as_posix())
        self.assertNotIn('set injected', word)
        self.assertNotIn('[exit]', word)


if __name__ == '__main__':
    unittest.main()
