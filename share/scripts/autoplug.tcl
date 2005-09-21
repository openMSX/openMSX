# if this machine has a cassetteport, then automaticaly plug
# in the cassetteplayer

if { [lsearch [openmsx_info connector] "cassetteport"] != -1 } {
	plug cassetteport cassetteplayer
}
