namespace eval showdebuggable {

#
# show_debuggable
#
set_help_text showdebuggable \
{Print content of debuggable nicely formatted

Usage:
   showdebuggable <debuggable> <address> [<linecount>] [<columns>]
}

set_tabcompletion_proc showdebuggable [namespace code tab_showdebuggable]
proc tab_showdebuggable {args} {
	if {[llength $args] == 2} {
		return [debug list]
	}
}

proc showdebuggable_line {debuggable address {columns 16}} {
	set size [debug size $debuggable]
	set num [expr {(($address + $columns) <= $size) ? $columns : ($size - $address)}]
	set mem "[debug read_block $debuggable $address $num]"
	binary scan $mem c* values
	set hex ""
	foreach val $values {
		append hex [format "%02x " [expr {$val & 0xff}]]
	}
	set pad [string repeat "   " [expr {$columns - $num}]]
	set asc [regsub -all {[^ !-~]} $mem {.}]
	return [format "%04x: %s%s %s\n" $address $hex $pad $asc]
}

proc showdebuggable {debuggable {address 0} {lines 8} {columns 16}} {
	set result ""
	for {set i 0} {$i < $lines} {incr i} {
		if {$address >= [debug size $debuggable]} break
		append result [showdebuggable_line $debuggable $address $columns]
		incr address $columns
	}
	return $result
}

# Some stuff for backwards compatibility. Do we want to deprecate them?
# I prefer to keep this as a convenience function.
set_help_text showmem \
{Print the content of the CPU visible memory nicely formatted
This is a shortcut for 'showdebuggable memory <address> <linecount> <columns>'.

Usage:
   showmem <address> [<linecount>] [<columns>]
}

proc showmem {{address 0} {lines 8} {columns 16}} {
	showdebuggable memory $address $lines $columns
}

namespace export showdebuggable
namespace export showmem

} ;# namespace showdebuggable

namespace import showdebuggable::*
