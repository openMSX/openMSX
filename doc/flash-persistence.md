# Flash persistence with current ROM contents

This prototype addresses [openMSX issue #2083](https://github.com/openMSX/openMSX/issues/2083): an older persistent Flash image must not hide rebuilt ROM code while preserving the game's Flash saves.

It builds on Laurens Holst (Grauw)'s sector-tracking design in [PR #1629](https://github.com/openMSX/openMSX/pull/1629) and his [October 2026 storage proposal](https://github.com/openMSX/openMSX/pull/2206#issuecomment-6239682987). Persistence now uses standard openMSX serialization, a complete chip image, an exact chip-type identifier and per-sector modification history. This remains a proposal for upstream review.

## Behavior

- Sectors ever programmed or erased by the guest retain their saved contents. Erasing an already-FF sector still counts as a modification.
- With a current ROM image, untouched sectors are initialized from that ROM, with FF beyond its end. Keeping a complete persistent image does not let its stale code override the current ROM.
- If an optional factory ROM is unavailable, a complete persistent image supplies the untouched sectors too. With neither source, initial contents are FF.
- Flash command timing is unchanged. Modification flags are set when program/erase operations execute, not when the command is issued.
- Ordinary savestates and reverse snapshots retain both the chip contents and volatile command/timer state. Disk persistence contains only nonvolatile data and modification history.
- Dirty data is saved after five seconds of real time and on destruction. A fresh profile with no writes creates no save file.
- Serialization completes in a temporary file beside the destination. Only after the archive closes successfully is it substituted for the previous save. This preserves the previous file if writing/replacement fails; it is not a guarantee against power loss on every filesystem.

Tracking is at erase-sector granularity. Updated code within a sector the guest has modified is deliberately hidden by the saved sector. Keep code and save data in different erase sectors. ASCII16-X's S29GL064S uses eight 8 KiB sectors followed by 127 64 KiB sectors.

## Format and configuration

`<flashname>` selects the new persistence file independently of SRAM. Automatically generated cartridge configurations use the old SRAM filename plus `.flash`, for example `probe.rom.SRAM.flash`. Old extension configurations and saved machines without `<flashname>` derive the same name from `<sramname>`. A configuration with only `<flashname>` also works.

The file is an ordinary compressed openMSX XML archive. Its version-1 `flash` record contains:

| Field | Meaning |
| --- | --- |
| `chipType` | Stable chip model identifier, such as `S29GL064S70TFI040`; not just a potentially shared JEDEC ID |
| `sectorSizes` | Complete ordered sector geometry |
| `modifiedSectors` | One 0/1 flag per sector; includes erased sectors |
| `content` | Full chip contents, using the standard compressed binary-blob representation |

The record can be extended/versioned when nonvolatile protection bits, ECC or secure regions are emulated. This change does not implement those hardware features. Commands, status registers, pending operations and timers are not persisted across emulator sessions.

The loader validates version, chip type, geometry, flag count/values and exact decoded content length before applying data. A corrupt or incompatible file **prevents mounting** and is retained. Unlike silently ignoring a chip mismatch, this makes it impossible for that session to overwrite the incompatible save with fresh contents. Correct the cartridge/chip selection or explicitly choose a different `<flashname>` for a separate save.

## Migration

Load order is:

1. The new `<flashname>` file.
2. The previous prototype's `<sramname>.sparse` file, if present.
3. The legacy raw `<sramname>` file.

Only a missing file allows fallback. Malformed or incompatible data is not hidden by loading an older file. Earlier formats remain untouched; a subsequent guest write or state restore creates the new file.

Sparse files retain their original modification flags, including erased sectors, after checksum and geometry validation. Raw legacy images contain no modification history, so every writable sector is conservatively marked modified. This protects saves but retains old-ROM masking for those legacy files. Recovering their intended mask requires the matching original ROM or an explicit choice of save sectors; comparing against a newly rebuilt ROM would be unsafe. The older `.SRAM` plus `.meta` draft format is not imported.

AmdFlash savestate version 5 remains unchanged: existing version-5 snapshots keep their masks, and version-4 snapshots protect all writable sectors. Restoring a pre-write state persists an empty mask, preventing newer disk saves from unexpectedly reappearing. Older emulator builds do not read the new persistence file. Use an isolated profile while evaluating this revision; alternating versions against one profile can load stale older files.

## Developer restore

[`loadstate_dev`](development-state-restore.md) remains available to restore CPU/RAM/VRAM/mapper state and refresh untouched ASCII16-X sectors from rebuilt ROM assets. Normal state restore remains exact for Flash. Busy Flash commands and states without usable modification history are rejected without replacing the active machine or flushing the rejected machine's old saves.

The command has no default keyboard binding. Developers can opt in with `bind CTRL+SHIFT+F7 loadstate_dev`. No new Flash-management window is introduced by this revision.

## Tests

The scripts create synthetic ROMs and isolated profiles under `derived/`; no game, firmware or personal save is distributed. Supply Philips NMS8250 firmware locally.

```text
python Contrib/flash-persistence-test.py --openmsx /path/to/openmsx --firmware-dir /path/to/systemroms --ram-routines /path/to/flash-persistence-ram.bin --legacy-fixtures /path/to/flash-baseline
python Contrib/dev-restore-test.py --openmsx /path/to/openmsx --firmware-dir /path/to/systemroms --release-rom-before-rebuild
python Contrib/flash-device-test.py --openmsx /path/to/openmsx --firmware-dir /path/to/systemroms
```

The Flash regression covers rebuilt code, multiple saved sectors, erased sectors, pre-write and pending-operation snapshots, raw/v4 and sparse migrations, full-image precedence, incompatible chips with identical geometry, malformed records, and failed file replacement. Its format fixtures are independently generated. The optional RAM routines exercise Max Iwamoto's programming/erase/status routines and all 256 byte values. Build `Contrib/flash-persistence-ram.asm` with a compatible Z80 assembler.

The developer-restore test checks new decoded graphics with preserved CPU/RAM/VRAM/banks, IPS/gzip inputs, normal versus developer restore, named/default states and safe rejection. `--release-rom-before-rebuild` is needed on Windows builds without the separate ROM-file locking fix.

The device test uses the MFR SCC+ SD's M29W640GB with a synthetic extension and no SD cards. It checks complete-image fallback when no factory ROM exists, an explicit `<flashname>` without `<sramname>`, state restoration and refreshing from a newly supplied factory ROM. A synthetic Manbow2 configuration also checks full AM29F040B images with read-only sectors and rejection of modification flags targeting protected sectors.

Validated on Windows x64 Release / MSVC v145 on 2026-10-17:

- All 38 persistence sessions passed, including the supplied RAM routines and actual upstream version-4 fixtures.
- All four developer-restore cases passed: ASCII16, ASCII16-X, IPS and gzip. Busy/legacy rejection preserved the active machine and newer disk saves.
- All four additional device checks passed, covering M29W640GB full-image fallback and AM29F040B read-only sectors alongside the main S29GL064S tests.

This does not establish compatibility with every AmdFlash cartridge, reverse/replay scenario or non-Windows host. Protection-bit/ECC/secure-region hardware remains unimplemented.
