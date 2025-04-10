# Easily save breakpoints to and restore from a file

namespace eval breakpoints {

proc save {type filename} {
	set all [dict values [debug $type list]]
	set fh [open $filename "w"]
	puts $fh [join [dict values [debug $type list]] \n]
	close $fh
}

proc load {type filename} {
	set current_bps [dict values [debug $type list]]
	set fh [open $filename "r"]
	# compare and create breakpoints if not found
	foreach bp [split [read -nonewline $fh] \n] {
		if {$bp in $current_bps} continue
		debug $type create {*}$bp
	}
	close $fh
}

proc make_save_help {type name} {
	set body "return \"save_$type <filename>\n\nSave all $type to a given file.\""
	proc $name {args} $body
}

proc make_load_help {type name} {
	set body "return \"load_$type <filename>\n\nLoad only new $type from a given file, while preserving pre-existing ones if any.\""
	proc $name {args} $body
}

make_save_help breakpoints save_breakpoints_help
set_help_proc save_breakpoints breakpoints::save_breakpoints_help

make_save_help watchpoints save_watchpoints_help
set_help_proc save_watchpoints breakpoints::save_watchpoints_help

make_save_help conditions save_conditions_help
set_help_proc save_conditions breakpoints::save_conditions_help

make_load_help breakpoints load_breakpoints_help
set_help_proc load_breakpoints breakpoints::load_breakpoints_help

make_load_help watchpoints load_watchpoints_help
set_help_proc load_watchpoints breakpoints::load_watchpoints_help

make_load_help conditions load_conditions_help
set_help_proc load_conditions breakpoints::load_conditions_help

namespace export breakpoints

}

proc save_breakpoints {filename} {
	breakpoints::save breakpoint $filename
}

proc save_watchpoints {filename} {
	breakpoints::save watchpoint $filename
}

proc save_conditions {filename} {
	breakpoints::save condition $filename
}

proc load_breakpoints {filename} {
	breakpoints::load breakpoint $filename
}

proc load_watchpoints {filename} {
	breakpoints::load watchpoint $filename
}

proc load_conditions {filename} {
	breakpoints::load condition $filename
}
