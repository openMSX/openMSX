set __help_cycle \
{Cycle through the possible values of an enum setting.
'cycle_back' does the same as 'cycle', but it goes in the opposite direction.

Usage:
  cycle      <enum_setting> [<cycle_list>]
  cycle_back <enum_setting> [<cycle_list>]

Example:
  cycle scaler_algorithm
  cycle scaler_algorithm "hq2x hq2xlite"
}

set_help_text cycle      $__help_cycle
set_help_text cycle_back $__help_cycle
set_tabcompletion_proc cycle      __tab_cycle
set_tabcompletion_proc cycle_back __tab_cycle
proc __tab_cycle { args } {
	set result [list]
	foreach setting [openmsx_info setting] {
		set type [lindex [openmsx_info setting $setting] 0]
		if {($type == "enumeration") || ($type == "boolean")} {
			lappend result $setting
		}
	}
	return $result
}

proc cycle { setting {cycle_list {}} {step 1} } {
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
	set new [expr ($cur + $step) % [llength $cycle_list]]
	set ::$setting [lindex $cycle_list $new]
}

proc cycle_back { setting } {
	cycle $setting {} -1
}

set_help_text toggle \
{Toggles a boolean setting

Usage:
  toggle <boolean_setting>

Example:
  toggle fullscreen
}

set_tabcompletion_proc toggle __tab_cycle

proc toggle { setting } {
        cycle $setting "on off"
}
