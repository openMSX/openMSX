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

# Disable build-in firmware
#if [info exists frontswitch] { set frontswitch off }
