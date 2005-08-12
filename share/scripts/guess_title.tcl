proc guess_rom_title { ps ss } {
	# check device name at address #4000 in given slot
	set slots [split [slotmap] \n]
	set slot_name "$ps"
	if [openmsx_info issubslotted $ps] { append slot_name ".$ss" }
	set index [lsearch $slots "slot $slot_name:"]
	if {$index != -1} { 
		set rom [lrange [lindex $slots [expr $index + 2]] 1 end]
		if { $rom != "empty" } { return $rom }
	}
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
			if [openmsx_info isexternalslot $ps $ss] {
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
