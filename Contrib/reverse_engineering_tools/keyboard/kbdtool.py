#!/usr/bin/env python3
"""Tools to investigate the keyboard of an MSX (or SVI, Sega) machine in
openMSX: see README.md, or run with --help."""
import os
import sys

sys.dont_write_bytecode = True  # (keep the source tree clean)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kbdlib.cli import main  # noqa: E402

if __name__ == '__main__':
    main()
