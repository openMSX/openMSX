# dump <count> number of entries of the stack at SP
#
# Usage:
#    stack [<count>]
#
proc stack { { depth 8 } } {
	set stackpointer [expr [debug read "CPU regs" 22] * 256 + [debug read "CPU regs" 23]]
	for {set i 0} {$i < $depth} {incr i} {
		set val [expr [debug read memory $stackpointer] + 256 * [debug read memory [expr $stackpointer + 1 ]] ]
		puts "[format %04x $stackpointer]: [format %04x $val]"
		incr stackpointer
		incr stackpointer
	}
}
