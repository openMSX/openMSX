# helper functions
proc getPC {} {
	expr [debug read "CPU regs" 20] * 256 + \
	     [debug read "CPU regs" 21]
}
proc getSP {} {
	expr [debug read "CPU regs" 22] * 256 + \
	     [debug read "CPU regs" 23]
}

#
# Disassemble z80 instructions
#
# Usage:
#   disasm                Disassemble 8 instr starting at the currect PC
#   disasm <addr>         Disassemble 8 instr starting at address <adr>
#   disasm <addr> <num>   Disassemble <num> instr starting at address <addr>
#
proc disasm {{address -1} {num 8}} {
	if {$address == -1} { set address [getPC] }
	for {set i 0} {$i < $num} {incr i} {
		set l [debug disasm $address]
		append result [format "%04X  %s\n" $address [join $l]]
		set address [expr $address + [llength $l] - 1]
	}
	return $result
}


#
# Run to the specified address, if a breakpoint is reached earlier we stop
# at that breakpoint.
#
proc run_to {address} {
	after break "debug remove_bp $address"
	debug set_bp $address
	debug cont
}


#
# Step in
#
proc step_in {} {
	debug step
}


# TODO step_out   needs changes in openMSX itself


#
# Step over. Execute the next instruction but don't step into subroutines.
# Only 'call' or 'rst' instructions are stepped over. 'push xx / jp nn' sequences
# can in theory also be use as calls but these are not skipped by this command.
#
proc step_over {} {
	set address [getPC]
	set l [debug disasm $address]
	if [string match "call*" [lindex $l 0]] {
		run_to [expr $address + [llength $l] - 1]
	} elseif [string match "rst*" [lindex $l 0]] {
		run_to [expr $address + [llength $l] - 1]
	} else
		debug step
	}
}
