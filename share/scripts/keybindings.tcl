# Since we are now supporting specific platforms we need a
# solution to support key bindings for multiple devices.

variable is_dingux [string match dingux "[openmsx_info platform]"]

# cycle_machine
bind_default CTRL+PAGEUP cycle_machine
bind_default CTRL+PAGEDOWN cycle_back_machine

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
bind_default PAGEUP   -global "go_back_one_step"
bind_default PAGEDOWN -global "go_forward_one_step"

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

# copy/paste (use middle-click for all platforms and also something similar to
# CTRL-C/CTRL-V, but not exactly that, as these combinations are also used on
# MSX. By adding META, the combination will be so rarely used that we can
# assume it's OK).
bind_default "mouse button2 down" type_clipboard
if {$tcl_platform(os) eq "Darwin"} { ;# Mac
	bind_default "keyb META+C" copy_screen_to_clipboard
	bind_default "keyb META+V" type_clipboard
} else { ;# any other
	bind_default "keyb META+CTRL+C" copy_screen_to_clipboard
	bind_default "keyb META+CTRL+V" type_clipboard
}
