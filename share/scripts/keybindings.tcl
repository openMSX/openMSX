# Since we are now supporting specific platforms we need a
# solution to support key bindings for multiple devices.

variable is_dingoo [string match *-dingux* $::tcl_platform(osVersion)]
variable is_android true ;# TODO

# cycle_machine
bind_default CTRL+PAGEUP cycle_machine
bind_default CTRL+PAGEDOWN cycle_back_machine

# osd_keyboard
if {$is_dingoo} {
	# for Dingoo assign the start button to show the keyboard
	bind_default "keyb RETURN" toggle_osd_keyboard
} elseif {$is_android} {
	# Android maps one of the virtual keys to WORLD_95
	# listen to that one in order to show the keyboard
	bind_default "keyb WORLD_95" toggle_osd_keyboard
}

# osd_menu
if {$tcl_platform(os) eq "Darwin"} { ;# Mac
	bind_default "keyb META+O" main_menu_toggle
} elseif {$is_dingoo} { ;# Dingoo
	bind_default "keyb ESCAPE" main_menu_toggle ;# select button
	bind_default "keyb MENU"   main_menu_toggle ;# default: power+select
} else { ;# any other
	bind_default "keyb MENU"   main_menu_toggle
}

# osd_widgets
if {$is_dingoo} {
	bind_default TAB -repeat "volume_control -2"
	bind_default BACKSPACE -repeat "volume_control +2"
}

# reverse
#  note: you can't use bindings with modifiers like SHIFT, because they
#        will already stop the replay, as they are MSX keys as well
bind_default PAGEUP   -repeat "go_back_one_step"
bind_default PAGEDOWN -repeat "go_forward_one_step"

# savestate
if {$tcl_platform(os) eq "Darwin"} {
	bind_default META+S savestate
	bind_default META+R loadstate
} else {
	bind_default ALT+F8 savestate
	bind_default ALT+F7 loadstate
}

# vdrive
bind_default       ALT+F9  "vdrive diska"
bind_default SHIFT+ALT+F9  "vdrive diska -1"
bind_default       ALT+F10 "vdrive diskb"
bind_default SHIFT+ALT+F10 "vdrive diskb -1"
