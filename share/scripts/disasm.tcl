#
# Disassemble z80 instructions
#
# Usage:
#   disasm                Disassemble 8 instr starting at the currect PC
#   disasm <addr>         Disassemble 8 instr starting at address <adr>
#   disasm <addr> <num>   Disassemble <num> instr starting at address <addr>
#

proc disasm {{address -1} {num 8}} {
	
	if {$address == -1} {
		set address [expr [debug read "CPU regs" 20] * 256 + \
		                  [debug read "CPU regs" 21]]
	}
	for {set i 0} {$i < $num} {incr i} {
		set l [debug disasm $address]
		append result [format "%04X  %s\n" $address [join $l]]
		set address [expr $address + [llength $l] - 1]
	}
	return $result
}
