# $Id$

# Default key bindings
bind print "screenshot"
bind pause "toggle pause"
bind ctrl+pause "quit"
bind F9 "toggle throttle"
bind F10 "toggle console"
bind F11 "toggle mute"
bind F12 "toggle fullscreen"

# Backwards compatibility commands
proc decr { var { num 1 } } {
	uplevel incr $var [expr -$num]
}
proc restoredefault { var } {
	uplevel unset $var
}
proc alias { cmd body } {
	proc $cmd {} $body
}

# Resolve data files. First try user directory, if the file doesn't exist
# there try the system direcectory
proc data_file { file } {
	global env
	set user_file $env(OPENMSX_USER_DATA)/$file
	if [file exists $user_file] { return $user_file }
	return $env(OPENMSX_SYSTEM_DATA)/$file
}

# Source additional scripts
source [data_file scripts/multi_screenshot.tcl]

# Disable build-in firmware
#if [info exists frontswitch] { set frontswitch off }
