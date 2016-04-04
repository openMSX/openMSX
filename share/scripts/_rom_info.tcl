set_help_text rom_info\
{Prints information about the given ROM device, coming from the software
database. If no argument is given, the first found (external) ROM device is
shown.}

namespace eval rom_info {

proc tab {args} {
	set result [list]

	foreach device [machine_info device] {
		if {[lindex [machine_info device $device] 0] eq "ROM"} {
			lappend result $device
		}
	}
	return $result
}

set_tabcompletion_proc rom_info [namespace code tab]

proc getlist_rom_info {{romdevice ""}} {
	if {$romdevice eq ""} {
		set romdevice [guess_rom_device]
		if {$romdevice eq ""} {
			error "No (external) ROM device found"
		}
	}

	if {[catch {set device_info [machine_info device $romdevice]}]} {
		error "No such device: $romdevice"
	}

	set device_type [lindex [machine_info device $romdevice] 0]
	if {$device_type ne "ROM"} {
		error [format "Device is not of type ROM, but %s" $device_type]
	}

	if {[catch {set rominfo [openmsx_info software [lindex $device_info 2]]}]} {
		return
	}

	dict with rominfo {
		# dummy info for missing items
		foreach key [list year company] {
			if {[set $key] eq ""} {
				set $key "(info not available)"
			}
		}

		if {$original} {
			# this is an unmodified original dump
			set status [format "Unmodified dump (confirmed by %s)" $orig_type]
		} else {
			# not original or unknown
			switch $orig_type {
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

		return [list \
				"title"		$title \
				"year"		$year \
				"company"	$company \
				"country"	$country \
				"status"	$status \
				"remark"	$remark]
	}
}

proc rom_info {{romdevice ""}} {
	set rominfo [rom_info::getlist_rom_info $romdevice]

	if {$rominfo eq ""} {return "No ROM information available..."}

	append result "Title:    [dict get $rominfo title]\n" \
				  "Year:     [dict get $rominfo year]\n" \
				  "Company:  [dict get $rominfo company]\n" \
				  "Country:  [dict get $rominfo country]\n" \
				  "Status:   [dict get $rominfo status]" \

	set remark [dict get $rominfo remark]

	if {$remark ne ""} {
		append result "\nRemark:   $remark"
	} else {
		append result "\nRemark:   None"
	}

	return $result
}

namespace export rom_info
namespace export getlist_rom_info

} ;# namespace rom_info

namespace import rom_info::*
