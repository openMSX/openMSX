set_help_text rom_info\
{Prints information about the given ROM device, coming from the software
database. If no argument is given, the first found ROM device is shown.}

namespace eval rom_info {

proc tab { args } {
	set result [list]

	foreach device [machine_info device] {
		if {[lindex [machine_info device $device] 0] == "ROM"} {
			lappend result $device
		}
	}
	return $result
}

set_tabcompletion_proc rom_info [namespace code tab]

proc rom_info { {romdevice ""} } {
	if {$romdevice == ""} {
		set romdevice [guess_rom_title]
		if {$romdevice == ""} {
			error "No ROM device given"
		}
	}

	if [catch {set device_info [machine_info device $romdevice]}] {
		error "No such device: $romdevice"
	}

	set device_type [lindex [machine_info device $romdevice] 0]
	if {$device_type != "ROM"} {
		error [format "Device is not of type ROM, but %s" $device_type]
	}

	if [catch {set rominfo [openmsx_info software [lindex $device_info 2]]}] {
		return "No ROM info available on $romdevice"
	}

	array set infomap $rominfo

	# dummy info for missing items
	foreach key [list year company] {
		if { $infomap($key) == "" } {
			set infomap($key) "(info not available)"
		}
	}

	if {$infomap(original)} {
		# this is an unmodified original dump
		set status [format "Unmodified dump (confirmed by %s)" $infomap(orig_type)]
	} else {
		# not original or unknown
		switch $infomap(orig_type) {
			"broken" {
				set status "Bad dump (game is broken)"
			}
			"translated" {
				set status "Translated from original"
			}
			"working" {
				set status "Modified but confirmed working"
			}
			default {
				set status "Unknown"
			}
		}
	}

	set result ""
	append result [format "Title:    %s\n" $infomap(title)]
	append result [format "Year:     %s\n" $infomap(year)]
	append result [format "Company:  %s\n" $infomap(company)]
	append result [format "Country:  %s\n" $infomap(country)]
	append result [format "Status:   %s" $status]
	if {$infomap(remark) != ""} {
		append result [format "\nRemark:   %s" $infomap(remark)]
	}
	return $result
}

namespace export rom_info

} ;# namespace rom_info

namespace import rom_info::*
