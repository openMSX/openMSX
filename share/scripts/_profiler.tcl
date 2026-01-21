# symboltracer: a profiler for the Probe/Trace viewer
#
# symboltracer uses the new Probe/Trace viewer as a profiler and symbol files
# to register functions.

namespace eval symboltracer {
namespace export start add stop
namespace ensemble create -prefixes 0

variable names {}         ;# user traces
variable counters {}      ;# count recursive calls
variable symbolfiles {}   ;# symbols file name

set_help_proc symboltracer [namespace code symboltracer_help]
proc symboltracer_help {args} {
	if {[llength $args] == 1} {
		return {symboltracer is a profiler that traces function calls in the Trace Viewer widget

Recognized commands: start, stop, add

Type 'help symboltracer <command>' for more information about each command.
}
    }
	switch -- [lindex $args 1] {
		"start" { return {Opens a symbol file and start tracing session

'start' reads a symbol file (like a .noi or .sym file) and creates breakpoints and traces for each entry found.

Syntax: symboltracer start [<sym-file>]
}}
		"add" { return {Creates breakpoint and trace for a user-defined name and address

'add' inserts a user-defined breakpoint and trace. No previously defined symbol is necessary. This is most useful for quick tracing sessions.

Syntax: symboltracer add <name> <address>
}}
		"stop" { return {Interrupts symboltracer session

'stop' removes all breakpoints and traces associated with symboltracer and all files included with the 'start' command.

Syntax: symboltracer stop
}}
	}
}

proc _enter_function {name} {
	# find function return address
	set retaddr [peek16 [reg SP]]
	# check if we create caller breakpoint for CALL or RST
	if {[peek [expr {$retaddr - 1}]] in "0 8 16 24 32 40 48 56" ||
	    [peek [expr {$retaddr - 3}]] in "196 204 205 212 220 228 236 244 252"} {
		variable counters
		dict incr counters $name
		set count [dict get $counters $name]
		debug trace add $name $count
		debug breakpoint create -once true -address $retaddr -command [namespace code "_exit_function {$name}"]
	}
}

proc _exit_function {name} {
	variable counters
	# ignore a probable dangling breakpoint from a previous session
	if {![dict exists $counters $name]} { return }
	dict incr counters $name -1
	set count [dict get $counters $name]
	if {$count == 0} {
		dict unset counters $name
		debug trace add $name
	} else {
		debug trace add $name $count
	}
}

proc add {name addr} {
	# add user-defined name/addr pair and create function breakpoints and traces
	variable names
	if {![dict exists $names $name]} {
		dict set names $name {}
		debug trace add $name
		debug breakpoint create -address $addr -command [namespace code "_enter_function {$name}"]
	}
}

proc add_symbol_set {symbols} {
	# run through collection of symbols
	foreach entry $symbols {
		dict with entry {
			add $name $value
		}
	}
}

proc start {{file ""}} {
	set symbols {}
	if {$file eq ""} {
		# use current symbols only
		set symbols [debug symbols lookup]
	} else {
		# load symbol file
		debug symbols load $file
		set symbols [debug symbols lookup -filename $file]
		variable symbolfiles
		lappend symbolfiles $file
	}
	if {[llength $symbols] == 0} {
		error "no symbols found"
	}
	add_symbol_set $symbols
}

proc stop {} {
	variable counters
	set counters {}

	variable names
	dict for {name _} $names {
		debug trace drop $name
	}
	set names {}

	dict for {bp entry} [debug breakpoint list] {
		if {[string match "* [namespace current] *" [dict get $entry "-command"]]} {
			catch {debug breakpoint remove $bp}
		}
	}

	variable symbolfiles
	foreach file $symbolfiles {
		debug symbols remove $file
	}
	set symbolfiles {}
}

set_tabcompletion_proc symboltracer [namespace code _tab_symboltracer]

proc _tab_symboltracer {args} {
	list "start" "add" "stop"
}

} ;# namespace symboltracer

namespace import symboltracer::*
