namespace eval pause_on_lost_focus {

variable paused_due_to_lost_focus false

proc lost_focus {} {
	variable paused_due_to_lost_focus
	if {!$::pause} {
		set ::pause on
		# CAUTION: this must be set after changing the pause setting!
		set paused_due_to_lost_focus true
	} else {
		set paused_due_to_lost_focus false
	}
}

proc gained_focus {} {
	variable paused_due_to_lost_focus
	if {$paused_due_to_lost_focus} {
		set ::pause off
		# NOTE: paused_due_to_lost_focus will be set to false by setting_changed
	}
}

proc pause_setting_changed {name1 name2 op} {
	variable paused_due_to_lost_focus
	set paused_due_to_lost_focus false
}

proc setting_changed {name1 name2 op} {
	variable paused_due_to_lost_focus
	set paused_due_to_lost_focus false
	if {$::pause_on_lost_focus} {
		bind_default "focus 0" "pause_on_lost_focus::lost_focus"
		bind_default "focus 1" "pause_on_lost_focus::gained_focus"
	} else {
		unbind_default "focus 0"
		unbind_default "focus 1"
	}
}

user_setting create boolean pause_on_lost_focus "pause emulation when the openMSX window loses focus" false
trace add variable ::pause_on_lost_focus write [namespace code setting_changed]
trace add variable ::pause write [namespace code pause_setting_changed]

# initial setup (bind or unbind)
setting_changed pause_on_lost_focus "" write

}
