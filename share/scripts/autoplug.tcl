# if this machine has a cassetteport, then automatically plug
# in the cassetteplayer
# on a Dingoo, automatically plug in the keyjoystick and if there are real
# joysticks, plug them as well for non-Dingoo platofrms

namespace eval autoplug {

proc plug_if_empty {connector pluggable} {
	if {[string first "--empty--" [plug $connector]] != -1} {
		# only when nothing already plugged
		plug $connector $pluggable
	}
}

proc do_autoplug {} {
	set connectors [list]
	catch {
		#can fail when you activate an 'empty' machine
		set connectors [machine_info connector]
	}
	set pluggables [list]
	catch {
		#can fail when you activate an 'empty' machine
		set pluggables [machine_info pluggable]
	}

	# cassette port
	if {"cassetteport" in $connectors} {
		plug_if_empty cassetteport cassetteplayer
	}

	# joystick ports
	if {[string match *-dingux* $::tcl_platform(osVersion)]} { ;# Dingoo
		if {"joyporta" in $connectors} {
			set ::keyjoystick1.triga LCTRL
			set ::keyjoystick1.trigb LALT
			plug_if_empty joyporta keyjoystick1
		}
	} else {
		if {("joyporta" in $connectors) &&
		    ("joystick1" in $pluggables)} {
			plug_if_empty joyporta joystick1
		}
		if {("joyportb" in $connectors) &&
		    ("joystick2" in $pluggables)} {
			plug_if_empty joyportb joystick2
		}
	}

	after boot [namespace code do_autoplug]
}

};# namespace autoplug

after boot autoplug::do_autoplug
