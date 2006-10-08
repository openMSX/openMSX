#
# show_debuggable
#
set_help_text showdebuggable \
{Print content of debuggable nicely formatted

Usage:
   showdebuggable <address> [<linecount>]
}

proc showdebuggable_line {debuggable address } {
	set data ""
	for {set i 0} {$i < 16} {incr i} {
		set data "$data [format %02x [debug read $debuggable [expr $address+$i]]]"
	}
	return "[format %04x $address]:$data"
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
