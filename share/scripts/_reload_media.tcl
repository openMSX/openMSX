set_help_text reload_media \
{Reload file-backed media by saving and restoring the current machine, without
resetting its CPU or RAM. Useful after rebuilding compatible ROM/disk assets.
Use uncompressed disks: compressed media can retain cached content.
Normal save-state rules apply: changed disks may become write-protected, and
Flash contents stored inside a state are preserved (use loadstate_dev for the
separate ASCII16-X Flash development workflow). Already loaded RAM is unchanged.
}

proc reload_media {} {
	set currentID [machine]
	if {$currentID eq ""} {error "No active machine."}
	set channel [file tempfile filename]
	close $channel
	try {
		store_machine $currentID $filename
		# Restore fully before destroying the original machine. A failed
		# restore leaves the original running, just like normal loadstate.
		set newID [restore_machine $filename]
	} finally {
		file delete -- $filename
	}
	delete_machine $currentID
	activate_machine $newID
	return $newID
}
