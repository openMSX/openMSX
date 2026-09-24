# Reloading a rebuilt ROM on Windows

Addresses [issue #955](https://github.com/openMSX/openMSX/issues/955).
Windows host tools cannot normally truncate or replace an inserted ROM while
openMSX retains its file mapping. After resolving patches, filenames, hashes
and ROM windows, this change copies file-backed ROM bytes to owned storage,
releases the mapping and closes the file. Running emulation retains a stable
snapshot while the host ROM is rebuilt. Non-Windows ROM handling, internal
ROM slices, disks and tapes are unchanged.

The cost is one owned buffer per file-backed ROM, including firmware. CPU
access remains through the same contiguous ROM data and mapper caches.
Normal Reset continues using the current snapshot. Save-state ROM checks are
retained; save states still depend on the original ROM files.

## Reload and reset

Press **Ctrl+Shift+R**, or use `reload_rom` in the console, to reinsert the current
ROM from disk and reset the MSX. This retains its slot, mapper and IPS patches.
With multiple ROM cartridges, specify `reload_rom carta` or `reload_rom cartb`.
Missing, empty or unreadable ROM/patch files are rejected before removal.
Other insertion failures can still occur after removal, such as corrupt gzip
contents. The binding is a default and does not replace user overrides.

Flash persistence follows the existing mapper behavior. This patch does not
solve older full-chip persistent Flash data masking a rebuilt ROM; that is
tracked separately in [#2083](https://github.com/openMSX/openMSX/issues/2083).
Nor does it refresh the Flash bytes embedded in an emulator save state.

## Validation

Use synthetic cartridges and isolated profiles with local Philips NMS8250
firmware. No firmware, game ROMs or generated state files are distributed.

```text
python Contrib/rom-replacement-test.py --openmsx /path/to/openmsx --firmware-dir /path/to/systemroms
python Contrib/reload-rom-test.py --openmsx /path/to/openmsx --firmware-dir /path/to/systemroms
```

The first test covers ASCII16, ASCII16-X, gzip, IPS, unchanged-state restoration,
in-place writes, truncation, atomic replacement, rename, deletion, stable loaded
bytes, and reinsertion. Use `--baseline` with upstream to reproduce the lock.
The second covers reload/reset, ordinary Reset, mapper/IPS retention, file
validation, cartridge B and ambiguous cartridge selection.

Validated on Windows x64 Release with Visual Studio v145. The existing SHA1
file-pool cache uses whole-second timestamps; the reload test separates host
writes by over a second to avoid a stale reported hash. This patch does not
change that cache. Test logs remain in disposable directories under `derived/`.
