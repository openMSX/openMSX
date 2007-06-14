#
# show_debuggable
#
set_help_text showdebuggable \
{Print content of debuggable nicely formatted

Usage:
   showdebuggable <address> [<linecount>]
}

proc showdebuggable_line {debuggable address } {
	set mem "[debug read_block $debuggable $address 16]"
	binary scan $mem c* values
	set hex ""
	foreach val $values {
		append hex "[format %02x [expr $val & 0xff]] "
	}
	set asc [regsub -all {[^ !-~]} $mem {.}]
	return "[format %04x $address]: $hex $asc"
}

proc showdebuggable {debuggable address {lines 8}} {
	for {set i 0} {$i < $lines} {incr i} {
		puts [showdebuggable_line $debuggable $address]
		incr address 16
	}
}

# some stuff for backwards compatibility

proc showmem {address {lines 8}} {
	puts [showdebuggable memory $address $lines]
}

proc showmem_line {address } {
	puts [showdebuggable_line memory $address ]
}
