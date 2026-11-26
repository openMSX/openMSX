#!/usr/bin/env python3
"""Run the portable Flash regressions against a freshly built native emulator."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--openmsx', type=Path)
    args = parser.parse_args()
    executable = args.openmsx
    if executable is None:
        candidates = [*ROOT.glob('derived/*/bin/openmsx'),
                      *ROOT.glob('derived/*/install/openmsx.exe')]
        if len(candidates) != 1:
            parser.error(f'Expected one native build, found {candidates}; specify --openmsx')
        executable = candidates[0]
    executable = executable.resolve(strict=True)
    commands = [
        ['Contrib/rom-harness-validation-test.py'],
        ['Contrib/flash-persistence-test.py', '--synthetic-firmware'],
        ['Contrib/flash-device-test.py', '--synthetic-firmware'],
        ['Contrib/flash-lifecycle-test.py'],
    ]
    for command in commands:
        if command[0] != 'Contrib/rom-harness-validation-test.py':
            command += ['--openmsx', str(executable)]
        print('Running', ' '.join(command), flush=True)
        subprocess.run([sys.executable, *command], cwd=ROOT, check=True, timeout=240)


if __name__ == '__main__':
    main()
