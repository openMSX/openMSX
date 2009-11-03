# if this machine has a cassetteport, then automaticaly plug
# in the cassetteplayer

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
	after boot [namespace code do_autoplug]
}

};# namespace autoplug

autoplug::do_autoplug
