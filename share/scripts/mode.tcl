# TODO mode descriptions are not yet used in any way
# TODO tab completion for the mode setting, so we can remove the hardcoded mentioning of the existing modes

namespace eval mode {

## Mode switching infrastructure

variable modes_info
variable old_mode

user_setting create string mode \
"This setting switches between different openMSX modes. A mode is a set of settings (mostly keybindings, but also OSD widgets that are activated) that are most suitable for a certain task. Currently only mode 'normal' and 'tas' exist." \
normal

trace add variable ::mode write [namespace code mode_changed]
set old_mode $::mode

proc register { name enter_proc leave_proc description } {
	variable modes_info
	set modes_info($name) [list $enter_proc $leave_proc $description]
}

proc mode_changed { name1 name2 op } {
	variable modes_info
	variable old_mode

	set new_mode $::mode
	if {![info exists modes_info($new_mode)]} {
		# New mode doesn't exist, restore variable and give an error
		set ::mode $old_mode
		error "No such mode: $new_mode. Possible values are: [array names modes_info]"
	}

	# Leave old mode
	eval [lindex $modes_info($old_mode) 1]

	# Enter new mode
	eval [lindex $modes_info($new_mode) 0]

	set old_mode $new_mode
}



## Define normal mode

proc enter_normal_mode {} {}
proc leave_normal_mode {} {}

register "normal" [namespace code enter_normal_mode] \
                  [namespace code leave_normal_mode] \
         "Mode that is most general purpose."



## Define tas mode

proc enter_tas_mode {} {
	if {![osd exists framecount]} {
		toggle_frame_counter
	}
	if {![osd exists cursors]} {
		toggle_cursors
	}
	reverse_widgets::enable_reversebar false

	# load/select/save reverse slot
	# Note: this hides the MSX keys 'SELECT' and 'STOP'
	bind_default F6 tas::load_replay_slot
	bind_default F7 tas::open_select_slot_menu
	bind_default F8 tas::save_replay_slot

	# Set up frame advance/reverse
	bind_default END -repeat advance_frame
	bind_default SCROLLOCK -repeat reverse_frame

	puts "WARNING: TAS mode is still very experimental and will almost certainly change next release!"
}

proc leave_tas_mode {} {
	if [osd exists framecount] {
		toggle_frame_counter
	}
	if [osd exists cursors] {
		toggle_cursors
	}
	# Leave reverse enabled, including bar

	# Remove reverse slot stuff
	unbind_default F6
	unbind_default F7
	unbind_default F8

	# Remove frame advance/reverse
	unbind_default END
	unbind_default SCROLLOCK
}

register "tas" [namespace code enter_tas_mode] \
               [namespace code leave_tas_mode] \
         "Mode for doing Tool Assisted Speedruns, with TAS widgets and easy ways to save replays."



## Enter initial mode

set old_mode $::mode
after realtime 0 { eval [lindex $mode::modes_info($::mode) 0] }

}
