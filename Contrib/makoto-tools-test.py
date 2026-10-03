"""Report-file safety checks; no emulator or machine firmware is needed."""
import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('makoto_benchmark', ROOT / 'Contrib/makoto-benchmark.py')
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)


class ReportSafetyTest(unittest.TestCase):
    def invoke(self, directory, emulator):
        argv = ['makoto-benchmark.py', '--baseline', 'unused-baseline',
                '--candidate', 'unused-candidate', '--firmware-dir', 'unused-firmware']
        with patch.object(sys, 'argv', argv), \
             patch.object(benchmark.tempfile, 'mkdtemp', return_value=str(directory)), \
             patch.object(benchmark.m, 'Emulator', emulator):
            benchmark.main()

    def test_existing_file_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            report = directory / 'results.json'
            report.write_text('keep this')
            def emulator(*args):
                self.fail('An emulator started before exclusive report creation')
            with self.assertRaises(FileExistsError):
                self.invoke(directory, emulator)
            self.assertEqual(report.read_text(), 'keep this')

    def test_link_to_another_file_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            sentinel = parent / 'outside-report.txt'
            sentinel.write_text('keep this too')
            directory = parent / 'run'
            directory.mkdir()
            os.link(sentinel, directory / 'results.json')
            def emulator(*args):
                self.fail('An emulator started before exclusive report creation')
            with self.assertRaises(FileExistsError):
                self.invoke(directory, emulator)
            self.assertEqual(sentinel.read_text(), 'keep this too')

    def test_report_exists_before_launch_and_survives_launch_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            report = directory / 'results.json'
            def emulator(*args):
                self.assertTrue(report.is_file())
                raise RuntimeError('Synthetic launch failure')
            with self.assertRaisesRegex(RuntimeError, 'Synthetic launch failure'):
                self.invoke(directory, emulator)
            self.assertEqual(json.loads(report.read_text())['runs'], [])
            # On Windows this also checks that the report handle was closed.
            report.rename(directory / 'closed.json')


if __name__ == '__main__':
    unittest.main()
