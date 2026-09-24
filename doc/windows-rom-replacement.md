# Reloading development media on Windows

Following [Wouter's review of PR #2205](https://github.com/openMSX/openMSX/pull/2205#issuecomment-5814276806), this revision removes Windows memory mapping from MappedFile. Windows uses a buffered read, while platforms with mmap retain that path. Rom now keeps its File handle only during initialization. This removes the earlier extra map/copy/unmap step and releases the host ROM file after loading, including firmware.

The buffered fallback reads from the start of the file, preserves its file position, supplies the requested zero padding, and frees its allocation if reading fails. Compressed ROMs still use an owned writable buffer, so closing the File does not invalidate their data.

## Refresh without resetting

Use `reload_media` in the openMSX console after editing compatible ROM or disk assets. It stores the current machine in a unique temporary state, restores it, then replaces the original machine and removes the temporary state. It preserves CPU registers, RAM and emulated time. Restore errors leave the original machine selected. It does not overwrite your quicksave.

This is Wouter's save/restore proposal. It also permits the translation workflow of restoring an earlier normal state immediately before a game reads the edited disk asset. Data already copied into guest RAM stays unchanged; the game needs to read the asset again.

## Existing development commands

For compatibility with the published release, Ctrl+Shift+R / `reload_rom` still reloads a selected cartridge and resets. `reload_rom carta` / `reload_rom cartb` retain mapper and IPS configuration. This remains useful for Flash cartridges because a simple save/restore preserves their old Flash contents.

In the combined fork, Ctrl+Shift+F7 / `loadstate_dev` remains the separate ASCII16-X workflow for refreshing untouched Flash sectors in an older state. The no-reset media command does not replace Flash persistence or this special development restore.

## Verified limits

- A mounted, writable ordinary DSK uses a live file handle. Windows permits in-place editing, but atomic replacement/rename can still fail. Removing file mappings alone does not remove that handle. Eject the disk before tools that replace its file, then reinsert or restore the state. Do not perform guest disk writes concurrently with a host editor.
- Compressed DSK images can retain old content through the shared decompression cache while the original machine is still alive. Use an uncompressed DSK for this development workflow. This is reproduced by the regression probe, not fixed in this revision.
- Normal state restore may write-protect a disk when its checksum changed. The existing warning/protection remains; deliberately eject/reinsert to allow writes again.
- AmdFlash devices serialize Flash contents, so state restore alone does not refresh them. Ordinary ROMs and ordinary DSKs were tested; this revision does not claim coverage of every media/device type.
- Existing whole-second FilePool timestamps can produce stale identity hashes for very rapid rewrites. Tests advance modification times to avoid that independent cache issue.
- Non-Windows builds have not been executed for this local revision.

## Tests

All fixtures are generated; firmware is supplied locally and tests use disposable profiles.

```text
python Contrib/rom-replacement-test.py --openmsx /path/to/openmsx.exe --firmware-dir /path/to/systemroms
python Contrib/reload-rom-test.py --openmsx /path/to/openmsx.exe --firmware-dir /path/to/systemroms
python Contrib/reload-media-test.py --openmsx /path/to/openmsx.exe --firmware-dir /path/to/systemroms
```

The replacement suite covers ASCII16, ASCII16-X, Yamanooto, gzip and IPS, host overwrite/truncate/replace/rename/delete, state roundtrips and hashes. The existing reload suite covers mapper/IPS retention, reset behavior, invalid paths and slot B. The no-reset suite verifies raw/gzip ROM refresh, CPU/RAM/time preservation, injected restore failure, ordinary disk updates, and an older-state disk-asset reload. Its compressed-disk case reports a known limitation explicitly, rather than counting it as a passing refresh.

The separate Flash fork also runs persistence suites for ASCII16-X and Yamanooto; see [Yamanooto validation](https://github.com/maxiwamoto/openMSX/blob/master/doc/yamanooto-validation.md).
