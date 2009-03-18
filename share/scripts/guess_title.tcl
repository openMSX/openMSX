set_help_text guess_title \
{Guess the title of the current game.
This proc uses some heuristics to guess the name of the current game, based on
the inserted ROM cartridges or disks.
}

proc guess_rom_title { ps ss } {
	# check device name at address #4000 in given slot
	set rom [machine_info slot $ps $ss 1]
	if { $rom != "empty" } { return $rom }
	return ""
}

proc guess_disk_title { drive_name } {
	# check name of the diskimage (remove directoy part and extension)
	set disk ""
	catch { set disk [lindex [$drive_name] 1] }
	set first [string last  "/" $disk]
	set last  [string first "." $disk $first]
	return [string range $disk [expr $first + 1] [expr $last - 1]]
}

proc guess_title { { fallback "" } } {
	# first try external slots
	for { set ps 0} { $ps < 4 } { incr ps } {
		for { set ss 0 } { $ss < 4 } { incr ss } {
			if [machine_info isexternalslot $ps $ss] {
				set title [guess_rom_title $ps $ss]
				if { $title != "" } { return $title }
			}
		}
	}

	# then try disks
	foreach drive [list "diska" "diskb"] {
		set title [guess_disk_title $drive]
		if { $title != "" } { return $title }
	}

	# guess failed, return fallback
	return $fallback
}
