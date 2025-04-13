# Easily save breakpoints to and restore from a file

proc save_breakpoints {filename} {
	set fh [open $filename "w"]
	foreach type {breakpoint watchpoint condition} {
		puts $fh "\[$type\]\n[join [dict values [debug $type list]] \n]\n"
	}
	close $fh
}

proc load_breakpoints {filename} {
	foreach type {breakpoint watchpoint condition} {
		set existing_bps($type) [dict values [debug $type list]]
	}
	set section {}
	set fh [open $filename "r"]
	foreach line [split [read -nonewline $fh] \n] {
		set line [string trim $line]
		# load valid file section only
		if {[regexp {^\[([^\]]+)\]$} $line -> section]} continue
		if {!($section in {breakpoint watchpoint condition})} continue
		# compare and create breakpoints if not found
		if {$line eq {} || $line in $existing_bps($section)} continue
		debug $section create {*}$line
	}
	close $fh
}

proc save_breakpoints_help {args} {
	return \
{save_breakpoints <filename>

Save all breakpoint subtypes (breakpoints, watchpoint, conditions) to a given file.
}}
set_help_proc save_breakpoints breakpoints::save_breakpoints_help

proc load_breakpoints_help {args} {
	return \
{load_breakpoints <filename>

Load all breakpoint subtypes (breakpoints, watchpoints and conditions) from a given file.
}}
set_help_proc load_breakpoints breakpoints::load_breakpoints_help
