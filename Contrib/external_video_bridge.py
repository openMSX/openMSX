#!/usr/bin/env python3
"""Run the external video bridge shipped with openMSX's shared scripts."""

from pathlib import Path
import runpy

runpy.run_path(str(Path(__file__).resolve().parents[1] /
                   "share/scripts/external_video_bridge.py"), run_name="__main__")
