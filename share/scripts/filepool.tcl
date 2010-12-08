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
		"reset"  { filepool_reset }
		"default" {
			error "Invalid subcommand, expected 'list', 'add' or 'remove', but got '$cmd'"
		}
	}
}

proc filepool_list {}  {
	set result ""
	set i 1
	foreach pool $::__filepool {
		array set a $pool
		append result "$i: $a(-path)  \[$a(-types)\]\n"
		incr i
	}
	return $result
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

proc filepool_add { args } {
	set pos [llength $::__filepool]
	set path ""
	set types ""
	foreach {name value} $args {
		if {$name == "-position"} {
			set pos [expr $value - 1]
		} elseif {$name == "-path"} {
			set path $value
		} elseif {$name == "-types"} {
			filepool_checktypes $value
			set types $value
		} else {
			error "Unknown option: $name"
		}
	}
	if {($pos < 0) || ($pos > [llength $::__filepool])} {
		error "Value out of range: $id"
	}
	if {$path == ""} {
		error "Missing -path"
	}
	if {$types == ""} {
		error "Missing -types"
	}

	set newpool [list -path $path -types $types]
	if {$pos == [llength $::__filepool]} {
		lappend ::__filepool $newpool
	} else {
		set ::__filepool [lreplace $::__filepool $pos -1 $newpool]
	}
	return ""
}

proc filepool_remove { id } {
	if {($id < 1) || ($id > [llength $::__filepool])} {
		error "Value out of range: $id"
	}
	set idx [expr $id - 1]
	set ::__filepool [lreplace $::__filepool $idx $idx]
	return ""
}

proc filepool_reset {}  {
	unset ::__filepool
}
