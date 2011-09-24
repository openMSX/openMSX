set_help_text guess_title \
{Guess the title of the current game.
This proc uses some heuristics to guess the name of the current game, based on
the inserted ROM cartridges, disks or cassettes.
}

proc guess_rom_title {} {
	# try only external slots
	for {set ps 0} {$ps < 4} {incr ps} {
		for {set ss 0} {$ss < 4} {incr ss} {
			if {[machine_info isexternalslot $ps $ss]} {
				set title [guess_rom_title_in $ps $ss]
				if {$title ne ""} {return $title}
			}
		}
	}
}

proc guess_rom_title_in {ps ss} {
	# check device name at address #4000 in given slot
	#
	# Note: this kind of fails for cases where the ROM is in an extension
	# (so in external slot), but not actually running (e.g. the ROM from
	# the Philips Music Module, after starting up with ESC, or the FMPAC
	# ROM). The guesser will find it and report it as title. I thought of
	# two strategies to overcome this, but both are inadequate:
	# 1. check whether the found ROM is actually mapped into Z80 space by
	# checking the value of I/O port 0xA8 (bit 2 and 3 should match $ps).
	# Pretty neat: you really see what is actually running. But fails for
	# ROMs that copy themselves to RAM (e.g. Koei games, tape games
	# converted to ROM, etc.).
	# 2. check whether the ROM came from an extension. But then you discard
	# the ROM even when it is actually running (e.g. after _FMPAC)
	# So, if someone has a good idea for this, which is not too CPU
	# intensive, please go ahead.
	set rom [machine_info slot $ps $ss 1]
	if {$rom ne "empty"} {return $rom}
	return ""
}

proc guess_disk_title {drive_name} {
	# check name of the diskimage (remove directory part and extension)
	set disk ""
	catch {set disk [lindex [$drive_name] 1]}
	if {$disk eq ""} {return ""}
	set first [string last  "/" $disk]
	set last  [string first "." $disk $first]
	return [string range $disk [expr {$first + 1}] [expr {$last - 1}]]
}

proc guess_cassette_title {} {
	set cassette ""
	# check name of the cassette image (remove directory part and extension)
	catch {set cassette [lindex [cassetteplayer] 1]}
	if {$cassette eq ""} {return ""}
	set first [string last  "/" $cassette]
	set last  [string first "." $cassette $first]
	return [string range $cassette [expr {$first + 1}] [expr {$last - 1}]]
}

proc guess_title {{fallback ""}} {
	# first try ROMs
	set title [guess_rom_title]
	if {$title ne ""} {return $title}

	# then try disks
	foreach drive [list "diska" "diskb"] {
		set title [guess_disk_title $drive]
		if {$title ne ""} {return $title}
	}

	# then try cassette
	set title [guess_cassette_title]
	if {$title ne ""} {return $title}

	# guess failed, return fallback
	return $fallback
}
