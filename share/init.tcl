# Default key bindings
proc bind_helper {key command} {
	# only bind when not already bound
	if {[bind $key] == ""} {
		bind $key $command
	}
}
bind_helper print "screenshot"
bind_helper pause "toggle pause"
bind_helper ctrl+pause "quit"
bind_helper F9 "toggle throttle"
bind_helper F10 "toggle console"
bind_helper F11 "toggle mute"
bind_helper F12 "toggle fullscreen"

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

# source additional scripts
source [data_file scripts/multi_screenshot.tcl]

# Disable build-in firmware
#if [info exists frontswitch] { set frontswitch off }
