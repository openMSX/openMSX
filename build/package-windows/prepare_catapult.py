"""Strict, fail-fast script to map the Catapult artifact into the layout
expected by openMSX Windows packaging scripts. No robustness/fallbacks.
"""
from __future__ import annotations
import argparse
import shutil
import sys
from pathlib import Path

def compute_catapult_configuration(configuration: str) -> str:
    cfg = configuration.lower()
    if cfg == 'release':
        return 'Unicode Release'
    elif cfg in ('developer', 'debug'):
        return 'Unicode Debug'
    raise ValueError(f"Unknown configuration: {configuration}")

def main():
    parser = argparse.ArgumentParser(description='Prepare Catapult artifact strict mapping')
    parser.add_argument('artifact', help='Path to extracted Catapult artifact root directory')
    parser.add_argument('--out', '-o', default='build/package-windows/catapult-staging', help='Output staging directory')
    parser.add_argument('--platform', choices=['Win32', 'x64'], default='x64')
    parser.add_argument('--configuration', choices=['Release', 'Developer', 'Debug'], default='Release')
    args = parser.parse_args()

    art_root = Path(args.artifact).resolve()
    out_dir = Path(args.out).resolve()

    # Fail fast if source artifact structure is wrong
    print(f"Reading Catapult artifact from: {art_root}")
    if not art_root.is_dir():
        sys.exit(f"ERROR: Artifact root '{art_root}' is not a valid directory.")

    # Expecting the layout: Catapult/bin, Catapult/doc, Catapult/resources
    bin_dir = art_root / 'bin'
    doc_dir = art_root / 'doc'
    res_dir = art_root / 'resources'

    for required_path in (bin_dir, doc_dir, res_dir):
        if not required_path.is_dir():
            sys.exit(f"ERROR: Expected directory '{required_path}' missing from artifact.")

    catapult_exe = bin_dir / 'catapult.exe'
    if not catapult_exe.is_file():
        sys.exit(f"ERROR: Mandatory file '{catapult_exe}' not found.")

    # Clean / recreate output staging directory
    if out_dir.exists():
        print(f"Clearing existing output directory: {out_dir}")
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Copy top-level folders directly to staging root
    print("Copying bin, doc, resources to staging root...")
    shutil.copytree(bin_dir, out_dir / 'bin')
    shutil.copytree(doc_dir, out_dir / 'doc')
    shutil.copytree(res_dir, out_dir / 'resources')

    # 2. Create the exact derived/ layout required by packagewindows.py / packagemsi.py
    cat_cfg = compute_catapult_configuration(args.configuration)
    plat_key = 'Win32' if args.platform.lower() == 'win32' else 'x64'
    derived_install = out_dir / 'derived' / f"{plat_key}-VC-{cat_cfg}" / 'install'
    derived_install.mkdir(parents=True, exist_ok=True)

    # 3. Place catapult.exe into the derived install path where WiX scripts look for it
    dest_exe = derived_install / 'catapult.exe'
    shutil.copy2(catapult_exe, dest_exe)
    print(f"Successfully staged catapult.exe -> {dest_exe}")

    print(f"\nPreparation complete. Staging folder ready at: {out_dir}")

if __name__ == '__main__':
    main()
