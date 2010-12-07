# TODO: this is still very much work-in-progress. Still needs:
#   help text
#   tab completion
#   better error handling
#   put stuff in a namespace
# Is the syntax for the command fine?

proc filepool { args } {
	set cmd [lindex $args 0]
	set args [lrange $args 1 end]
	switch -- $cmd {
		"list"   { filepool_list }
		"add"    { eval filepool_add    $args }
		"remove" { filepool_remove $args }
		"default" {
			error "Invalid subcommand, expected 'list', 'add' or 'remove', but got '$cmd'"
		}
	}
}

proc filepool_decode {} {
	set result [list]
	foreach pool $::__filepool {
		array set a $pool
		lappend result [list $a(-priority) $a(-path) $a(-types)]
	}
	return $result
}

proc filepool_encode { pools } {
	set result [list]
	foreach pool $pools {
		foreach {priority path types} $pool {}
		lappend result [list -priority $priority -path $path -types $types]
	}
	set ::__filepool $result
}

proc filepool_search { pools id } {
	set i 0
	foreach pool $pools {
		if {[lindex $pool 0] == $id} {
			return $i
		}
		incr i
	}
	return -1
}

proc filepool_checktypes { types } {
	set valid [list "system_rom" "rom" "disk" "tape"]
	foreach type $types {
		set idx [lsearch $valid $type]
		if {$idx == -1} {
			error "Invalid type, expected one of $valid, but got $type"
		}
	}
}

proc filepool_list {}  {
	set result ""
	foreach pool [lsort -index 0 [filepool_decode]] {
		foreach {priority path types} $pool {}
		append result "Priority: $priority  path: $path  types: $types\n"
	}
	return $result
}

proc filepool_add { args } {
	set priority ""
	set path ""
	set types ""
	foreach {name value} $args {
		if {$name == "-priority"} {
			set priority $value
		} elseif {$name == "-path"} {
			set path $value
		} elseif {$name == "-types"} {
			filepool_checktypes $value
			set types $value
		} else {
			error "Unknown option: $name"
		}
	}
	if {$priority == ""} {
		error "Missing -priority"
	}
	if {$path == ""} {
		error "Missing -path"
	}
	if {$types == ""} {
		error "Missing -types"
	}

	set pools [filepool_decode]
	set idx [filepool_search $pools $priority]
	if {$idx != -1} {
		error "Already got a pool with priority $priority"
	}
	lappend pools [list $priority $path $types]
	filepool_encode $pools
	return ""
}

proc filepool_remove { priority } {
	set pools [filepool_decode]
	set idx [filepool_search $pools $priority]
	if {$idx == -1} {
		error "No pool with priority $priority"
	}
	set pools [lreplace $pools $idx $idx]
	filepool_encode $pools
	return ""
}
