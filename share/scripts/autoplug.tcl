# if this machine has a cassetteport, then automatically plug
# in the cassetteplayer
# on a Dingoo, automatically plug in the keyjoystick and if there are real
# joysticks, plug them as well for non-Dingoo platofrms

namespace eval autoplug {

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
		if {[string first "--empty--" [plug cassetteport]] != -1} {
			# only when nothing already plugged
			plug cassetteport cassetteplayer
		}
	}

	# joystick ports
	if {[string match *-dingux* $::tcl_platform(osVersion)]} { ;# Dingoo
		if {"joyporta" in $connectors} {
			set ::keyjoystick1.triga LCTRL
			set ::keyjoystick1.trigb LALT
			if {[string first "--empty--" [plug joyporta]] != -1} {
				# only when nothing already plugged
				plug joyporta keyjoystick1
			}
		}
	} else {
		if {("joyporta" in $connectors) &&
		    ("joystick1" in $pluggables)} {
			if {[string first "--empty--" [plug joyporta]] != -1} {
				# only when nothing already plugged
				plug joyporta joystick1
			}
		}
		if {("joyportb" in $connectors) &&
		    ("joystick2" in $pluggables)} {
			if {[string first "--empty--" [plug joyportb]] != -1} {
				# only when nothing already plugged
				plug joyportb joystick2
			}
		}
	}

	after boot [namespace code do_autoplug]

	# See comment in reverse.tcl (search for 'after boot')
	reverse::after_switch
}

};# namespace autoplug

after boot autoplug::do_autoplug
