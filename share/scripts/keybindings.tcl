# Since we are now supporting specific platforms we need a
# solution to support key bindings for multiple devices.

if {[string match *-dingux* $::tcl_platform(osVersion)]} {
	bind_default TAB -repeat "volume_control -2"
	bind_default BACKSPACE -repeat "volume_control +2"
}
