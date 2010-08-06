# if this machine has a cassetteport, then automatically plug
# in the cassetteplayer
# on a Dingoo, automatically plug in the joystick 
# and assign shoulder keys [L/R] to the volume widget

namespace eval autoplug {

proc do_autoplug {} {
	set connectors [list]
	catch {
		#can fail when you activate an 'empty' machine
		set connectors [machine_info connector]
	}
	if {[lsearch $connectors "cassetteport"] != -1} {
		if {[string first "--empty--" [plug cassetteport]] != -1} {
			# only when nothing already plugged
			plug cassetteport cassetteplayer
		}
	}
	if {[string match *-dingux* $::tcl_platform(osVersion)]} { ;# Dingoo

		if {[lsearch $connectors "joyporta"] != -1} {
			set ::keyjoystick1.triga LCTRL
			set ::keyjoystick1.trigb LALT
			if {[string first "--empty--" [plug joyporta]] != -1} {
				# only when nothing already plugged
				plug joyporta keyjoystick1
			}
		}
	}
	after boot [namespace code do_autoplug]

	# See comment in reverse.tcl (search for 'after boot')
	reverse::after_switch
}

};# namespace autoplug

after boot autoplug::do_autoplug
