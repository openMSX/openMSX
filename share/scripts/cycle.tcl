set_help_text cycle \
{Cycle through the possible values of an enum setting

Usage:
  cycle <enum_setting> [<cycle_list>]

Example:
  cycle scaler_algorithm
  cycle scaler_algorithm "hq2x hq2xlite"
}

set_tabcompletion_proc cycle tab_cycle
proc tab_cycle { args } {
	set result [list]
	foreach setting [openmsx_info setting] {
		set type [lindex [openmsx_info setting $setting] 0]
		if {($type == "enumeration") || ($type == "boolean")} {
			lappend result $setting
		}
	}
	return $result
}

proc cycle { setting { cycle_list {}}} {
	set setting_info [openmsx_info setting $setting]
	set type [lindex $setting_info 0]
	if {$type == "enumeration"} {
		if {[llength $cycle_list] == 0} {
			set cycle_list [lindex $setting_info 2]
		}
	} elseif {$type == "boolean"} {
		set cycle_list [list "true" "false"]
	} else {
		error "Not an enumeration setting: $setting"
	}
	set cur [lsearch -exact $cycle_list [set ::$setting]]
	set new [expr ($cur + 1) % [llength $cycle_list]]
	set ::$setting [lindex $cycle_list $new]
}


set_help_text toggle \
{Toggles a boolean setting

Usage:
  toggle <boolean_setting>

Example:
  toggle fullscreen
}

set_tabcompletion_proc toggle tab_cycle

proc toggle { setting } {
        cycle $setting "on off"
}
