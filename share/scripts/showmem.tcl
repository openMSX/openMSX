#
# print memory nicely formatted
#
# Usage:
#    showmem <address> [<linecount>]
#

proc showmem_line { address } {
	set data ""
	for {set i 0} {$i < 16} {incr i} {
		set data "$data [format %02x [debug read memory [expr $address+$i]]]"
	}
	return "[format %04x $address]:$data"
}

proc showmem {address {lines 8}} {
	for {set i 0} {$i < $lines} {incr i} {
		puts [showmem_line $address]
		set address [expr $address + 16]
	}
}
