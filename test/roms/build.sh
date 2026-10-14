#!/bin/sh
# test/roms/build.sh
#
# (Re)build the MSX cartridge ROM fixtures for the step_back integration
# tests, using SjASMPlus (https://github.com/z00m128/sjasmplus).
#
# The assembler source uses `ORG $4000` and pads with `DS $8000-$, $FF`, so
# `sjasmplus --raw=<out>` emits a plain 16384-byte image (the $4000-$7FFF
# address space maps straight into the file, "AB" header at offset 0).
#
# Usage:
#    sh test/roms/build.sh            # default sjasmplus
#    SJASMPLUS=/path/to/sjasmplus sh test/roms/build.sh
#
# The resulting .rom files are checked in, so this is only needed when the
# .asm fixtures change.

set -e
cd "$(dirname "$0")"

SJASMPLUS="${SJASMPLUS:-sjasmplus}"
if ! command -v "$SJASMPLUS" >/dev/null 2>&1; then
	echo "error: sjasmplus not found (set SJASMPLUS=...)" >&2
	exit 1
fi

for base in ldir_irq otir_irq ldir_djnz_loop ldir_varying_bc ldir_nested_inir ldir_random_bc; do
	echo "building $base.rom"
	"$SJASMPLUS" --raw="$base.rom" "$base.asm" >/dev/null
done

echo "done"
