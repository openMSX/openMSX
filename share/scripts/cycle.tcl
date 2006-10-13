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
		if {[lindex [openmsx_info setting $setting] 0] == "enumeration" } {
			lappend result $setting
		}
	}
	return $result
}

proc cycle { setting { cycle_list {}}} {
        if { [llength $cycle_list] == 0 } {
	        set setting_info [openmsx_info setting $setting]
		if ![string equal [lindex $setting_info 0] "enumeration"] {
			error "Not an enumeration setting: $setting"
		}
		set cycle_list [lindex $setting_info 2]
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
