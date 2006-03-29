set_help_text cycle \
{Cycle through the possible values of an enum setting

Usage:
  cycle <enum_setting> [<cycle_list>]

Example:
  cycle scaler_algorithm
  cycle scaler_algorithm "hq2x hq2xlite"
}

proc cycle { setting { cycle_list {}}} {
        if { [llength $cycle_list] == 0 } {
	        if [catch { set cycle_list [openmsx_info $setting] } ] {
		        error "Not an enum setting: $setting"
	        }
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

proc toggle { setting } {
        cycle $setting "on off"
}
