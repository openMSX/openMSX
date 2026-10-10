# Restore a test point with current ROM assets

`loadstate_dev [name]` restores a saved machine without resetting it, then
refreshes untouched ASCII16-X Flash sectors from the current ROM image before
execution resumes. The default state is `quicksave`. A default **Ctrl+Shift+F7**
binding invokes it; normal `loadstate` and reverse/replay are unchanged.

On Windows, save a test point with **Alt+F8**, rebuild the ROM at the same path,
and press **Ctrl+Shift+F7**. Save before the image/text is unpacked: RAM, VRAM,
CPU registers and mapper banks are restored exactly, so already-decoded assets
remain old. The code, bank and RAM layouts must still be compatible.

To rebuild while the old ROM remains inserted on Windows, the separate ROM
snapshot fix for [#955](https://github.com/openMSX/openMSX/issues/955) is also
needed. Without it, eject the cartridge before rewriting its file, then use
the developer restore to resume the saved test point.

## Flash behavior

This depends on [modified-sector persistence](flash-persistence.md). Sectors
ever programmed or erased by the guest retain their snapshot contents. Other
sectors overlapping the current ROM image are refreshed from that image,
including IPS patches and gzip decoding. Bytes outside the ROM image remain
as saved. CPU read caches are invalidated. The source ROM and snapshot file
are unchanged; refreshed bytes are not marked as persistent guest writes.

Save progress comes from the snapshot, not from newer progress on disk. Ordinary
ROM mappers already read the current file during state restoration. Other Flash
mappers are not refreshed by this helper.

A Flash command in progress prevents refreshing. Legacy states without sector
history protect all writable sectors and are rejected when no ROM bytes can
be refreshed. Create a new state with this build rather than guessing which
old bytes belong to game code. On rejection, the active machine is retained
and the rejected machine's queued persistent writes are cancelled before its
destruction, preserving the active game's disk save.

## Implementation and tests

The Tcl command restores an inactive machine, calls
`debug refresh_flash_from_rom <device>` for each ASCII16-X cartridge, then
activates it. The low-level helper uses the ROM image loaded during restoration;
it does not reopen the file itself. `debug discard_flash_persistence <device>`
is used only to clean up a rejected inactive machine. Neither helper is
recorded for reverse/replay. Their API shape is proposed for review.

```text
python Contrib/dev-restore-test.py --openmsx /path/to/openmsx --firmware-dir /path/to/systemroms
```

On Windows without the ROM snapshot fix, add `--release-rom-before-rebuild`.
The test runs a tiny RAM-based graphics decoder after restoration and verifies
new pixels, preserved registers/RAM/VRAM/banks and saved Flash bytes. It covers
ASCII16, ASCII16-X, IPS, gzip, named/default states, unchanged ordinary restore,
missing states, busy Flash and legacy rejection without persistent-save damage.
It uses only generated ROMs and isolated profiles; supply firmware locally.
