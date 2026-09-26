# Flash sector persistence prototype

This prototype addresses [openMSX issue #2083](https://github.com/openMSX/openMSX/issues/2083): after Flash has been saved, a rebuilt ROM with the same filename can be hidden by the old full-chip `.SRAM` image.

The design builds on **Laurens Holst (Grauw)'s draft [PR #1629](https://github.com/openMSX/openMSX/pull/1629)**, including its save-path resolution helper. It is adapted to the current Flash command/timing implementation rather than cherry-picked. The draft's payload/XML pair is replaced here by a single checksummed file. This is an experimental local format, not an agreed upstream format. The format and migration policy are proposed for upstream review.

## Loading and saving

- Initialize unmodified sectors from the current ROM, or FF beyond the ROM.
- Overlay only sectors previously programmed or erased by the emulated Flash.
- Track entire erase sectors. An erase counts as a modification even when the result is all FF: subsequent ROM changes must not undo a saved erase.
- Keep the current Flash operation timing and command engine. Mark sectors when an operation actually changes/erases memory, not when its command is first issued.
- Flush after five seconds of real time and on device destruction when dirty.
- Write a temporary file beside the destination, flush and close it, then replace the destination. A replacement failure keeps the previous save and reports a warning. This is not a guarantee of power-loss durability on every host filesystem.
- No writes means no persistence file for a fresh profile.

A ROM update within a **modified sector** remains hidden by that sector's saved contents. Code and save data must occupy separate erase sectors. For ASCII16X's currently emulated S29GL064S, the first 64 KiB comprises eight 8 KiB sectors; the rest uses 64 KiB sectors.

## Files and compatibility

New data is stored beside the existing persistence filename, with `.sparse` appended (for example `probe.rom.SRAM.sparse`). Existing full-image `.SRAM` files are left intact. If both exist, the sparse file takes precedence.

The binary format is little-endian:

| Field | Size |
| --- | --- |
| Magic/version `OMSXFLS` followed by byte 01 | 8 bytes |
| Full Flash chip size | 4 bytes |
| Sector count | 4 bytes |
| Each sector: size, modified flag (0 or 1) | 5 bytes per sector |
| Modified sector payloads in sector order | Sum of modified sector sizes |
| CRC32 of all preceding bytes | 4 bytes |

The loader validates geometry, flags, bounds, payload length and checksum before applying any data. A corrupt file prevents mounting the cartridge rather than silently starting blank and overwriting progress.

For ASCII16X, one modified 64 KiB sector occupies **66,231 bytes**, versus the old 8 MiB full image.

### Existing saves

Legacy raw `.SRAM` files have no modification history. They load conservatively with **all writable sectors marked modified**. This protects existing data but also preserves the old-ROM masking behavior for those legacy files. It is not safe to guess which bytes are saves by comparing them against a newly rebuilt ROM. Migration that recovers the intended sparse mask would require the matching original ROM or an explicit choice of save sectors.

For a fresh development test, use a separate openMSX profile. Do not delete existing saves. Sparse output is created on a subsequent write or state restore; the legacy input is retained. Grauw's draft `.SRAM` + `.meta` format is deliberately not imported by this prototype.

AmdFlash save-state version 5 retains the previous RAM serialization and adds modified-sector flags. Version 4 states load with all writable sectors marked modified. Restoring a new state with no modified sectors writes an empty mask, so a previous persistent save does not reappear on restart. Older openMSX builds do not understand the new sparse file or version-5 states; keep a separate profile while testing. Do not alternate old and new builds against the same persistence files.

## Regression tests

`Contrib/flash-persistence-test.py` uses generated, disposable ROMs and a new isolated profile under `derived/`. It does not load or upload a commercial game. It requires locally supplied Philips NMS8250 firmware.

Build the supplied Z80 routines (tniASM 0.44 plus its compatibility layer is one option):

```text
tniasm.exe /path/to/compat.asm Contrib/flash-persistence-ram.asm derived/flash-persistence-ram.bin
```

The assembly is Max Iwamoto's hardware-tested byte-program, erase and DQ7/DQ5 polling code, with only entry-point jumps and assembler-compatible number spelling added. He reports it works on ASCII16X, MFR, C2 and C2+. Automated testing here focuses on ASCII16X; it does not independently verify those physical devices.

Test mapping:

- Flash commands in CPU page 1: `4AAAh` and `4555h`, bank 0.
- Flash save data in CPU page 2: `8000h-9FFFh` during this test, bank 8 (physical `020000h`). The test avoids mapper-register writes at `A000h-BFFFh`.
- Routines in RAM at `C200h`, source at `C800h`, RAM stack below `FE00h`; interrupts disabled.
- Program all 256 byte values, check carry and readback, erase, restart, and replace the ROM with a different code marker while retaining saves.
- DQ5 failure and DQ7 success branches are additionally tested against controlled RAM status values. This tests the assembly's decision/reset path; it does not inject a hardware timing failure into the Flash model.

```text
python Contrib/flash-persistence-test.py --openmsx /path/to/patched/openmsx.exe --firmware-dir /path/to/systemroms --ram-routines derived/flash-persistence-ram.bin
```

To reproduce the original bug and produce actual version-4 save-state fixtures, run the same test with a clean upstream executable:

```text
python Contrib/flash-persistence-test.py --openmsx /path/to/upstream/openmsx.exe --firmware-dir /path/to/systemroms --baseline
```

Then pass its reported artifact directory to the patched run with `--legacy-fixtures /path/to/flash-baseline-...`.

Coverage includes new-ROM visibility, saved/erased sectors, empty-state restore, pending RAM-program state restore, malformed data rejection, legacy raw saves and v4 snapshots, and failed destination replacement on Windows. This does not yet establish compatibility across every cartridge using AmdFlash, reverse execution, or non-Windows hosts.

## Validation and developer restore

Validated on Windows x64 Release with Visual Studio v145. Tests use locally
supplied firmware and generated ROMs; firmware and save artifacts are not
included. The baseline reproduces whole-image masking. Regression coverage
includes RAM-executed programming/erase, old save fixtures and malformed files.
Non-Windows hosts and all other AmdFlash cartridges still need wider testing.

See [development restore](development-state-restore.md) for the optional
state-loading workflow that refreshes untouched Flash from current ROM assets.
