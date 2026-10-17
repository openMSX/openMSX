#!/usr/bin/env python3
"""Validate native-executable and Tcl-path boundaries in the ROM test harness."""
import base64
import os
from pathlib import Path
import runpy
import tempfile
import unittest

helpers = runpy.run_path(str(Path(__file__).with_name('rom-replacement-test.py')))
executable_path = helpers['executable_path']
tcl_path = helpers['tcl_path']


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
            if os.name == 'nt':
                fake_exe = root / 'openmsx.exe'
                fake_exe.write_text('@echo not a PE executable')
                with self.assertRaises(ValueError):
                    executable_path(fake_exe)

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
