# Since we are now supporting specific platforms we need a
# solution to support key bindings for multiple devices.

variable is_dingux [string match dingux "[openmsx_info platform]"]
variable is_android [string match android "[openmsx_info platform]"]

# cycle_machine
bind_default CTRL+PAGEUP cycle_machine
bind_default CTRL+PAGEDOWN cycle_back_machine

# osd_keyboard
if {$is_dingux} {
	# Use the SELECT button.
	bind_default "keyb ESCAPE" toggle_osd_keyboard
} elseif {$is_android} {
	# Android maps one of the virtual keys to WORLD_95
	# listen to that one in order to show the keyboard
	bind_default "keyb WORLD_95" toggle_osd_keyboard
}

# osd_menu
if {$tcl_platform(os) eq "Darwin"} { ;# Mac
	bind_default "keyb META+O" main_menu_toggle
} elseif {$is_dingux} { ;# OpenDingux
	bind_default "keyb RETURN" main_menu_toggle ;# START button
	bind_default "keyb HOME" main_menu_toggle ;# power slider flick
} else { ;# any other
	bind_default "keyb MENU"   main_menu_toggle
}

# pause
if {$is_dingux} {
	# Power slider lock position.
	bind_default "keyb PAUSE" "set pause on"
	bind_default "keyb PAUSE,release" "set pause off"
}

# osd_widgets
if {$is_dingux} {
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
