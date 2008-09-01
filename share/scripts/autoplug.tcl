# if this machine has a cassetteport, then automaticaly plug
# in the cassetteplayer

proc do_autoplug {} {
	if { [lsearch [machine_info connector] "cassetteport"] != -1 } {
		if {[string first "--empty--" [plug cassetteport]] != -1} {
			# only when nothing already plugged
			plug cassetteport cassetteplayer
		}
	}
	after machine_switch do_autoplug
}
do_autoplug
