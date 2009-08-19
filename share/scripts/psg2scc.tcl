namespace eval psg2scc {

#TODO:
#
# Translate PSG volume to SCC volume (log>linear)
#
# Make more waves

variable active false
variable cur_wp1
variable cur_wp2
variable latch -1
variable regs [list 0xa0 0xa1 0xa2 0xa3 0xa4 0xa5 -1 0xaf 0xaa 0xab 0xac -1 -1 -1 -1 -1]

proc init {} {
	for {set i 0} {$i < 32} {incr i} {
		set_scc_wave 0 3
		set_scc_wave 1 3
		set_scc_wave 2 3
	}
}

proc update1 {} {
	variable latch
	set latch $::wp_last_value
}

proc update2 {} {
	variable latch
	variable regs

	set reg [expr ($latch == -1) ? $latch : [lindex $regs $latch]]
	set val $::wp_last_value
	if {$latch == 7} { set val [expr ($val ^ 0x07) & 0x07] }
	if {$reg != -1} { debug write SCC\ SCC $reg $val }
}

proc toggle_psg2scc {} {
	variable active
	variable cur_wp1
	variable cur_wp2

	set active [expr !$active]
	if {$active} {
		init
		set cur_wp1 [debug set_watchpoint write_io 0xa0 1 { psg2scc::update1 }]
		set cur_wp2 [debug set_watchpoint write_io 0xa1 1 { psg2scc::update2 }]
	} else {
		debug remove_watchpoint $cur_wp1
		debug remove_watchpoint $cur_wp2
		debug write SCC\ SCC 0xaf 0
	}
}

proc set_scc_wave {channel form} {
	
	set base [expr $channel*32]
	
	#saw tooth
	if {$form==1} {
		for {set i 0;} {$i < 32} {incr i;} {
			debug write SCC\ SCC [expr $base+$i] [expr 255-($i*8)]
		}
	}
	
	#square
	if {$form==2} {
		for {set i 0;} {$i < 16} {incr i;} {
			debug write SCC\ SCC [expr $base+$i] 128
			debug write SCC\ SCC [expr $base+$i+15] 127
		}
	}

	#square
	if {$form==3} {
		for {set i 0;} {$i < 32} {incr i;} {
			debug write SCC\ SCC [expr $base+0] 255
			debug write SCC\ SCC [expr $base+1] [expr 256-(1*16)]
			debug write SCC\ SCC [expr $base+2] [expr 256-(2*16)]
			debug write SCC\ SCC [expr $base+3] [expr 256-(3*16)]
			debug write SCC\ SCC [expr $base+4] [expr 256-(4*16)]
			debug write SCC\ SCC [expr $base+5] [expr 256-(5*16)]
			debug write SCC\ SCC [expr $base+6] [expr 256-(6*16)]
			debug write SCC\ SCC [expr $base+7] [expr 256-(7*16)]
			debug write SCC\ SCC [expr $base+8] [expr 256-(8*16)]
			debug write SCC\ SCC [expr $base+9] [expr 256-(7*16)]
			debug write SCC\ SCC [expr $base+10] [expr 256-(6*16)]
			debug write SCC\ SCC [expr $base+11] [expr 256-(5*16)]
			debug write SCC\ SCC [expr $base+12] [expr 256-(4*16)]
			debug write SCC\ SCC [expr $base+13] [expr 256-(3*16)]
			debug write SCC\ SCC [expr $base+14] [expr 256-(2*16)]
			debug write SCC\ SCC [expr $base+15] [expr 256-(1*16)]

			debug write SCC\ SCC [expr $base+16] [expr 128-(7*16)]
			debug write SCC\ SCC [expr $base+17] [expr 128-(6*16)]
			debug write SCC\ SCC [expr $base+18] [expr 128-(5*16)]
			debug write SCC\ SCC [expr $base+19] [expr 128-(4*16)]
			debug write SCC\ SCC [expr $base+20] [expr 128-(3*16)]
			debug write SCC\ SCC [expr $base+21] [expr 128-(2*16)]
			debug write SCC\ SCC [expr $base+22] [expr 128-(1*16)]
			debug write SCC\ SCC [expr $base+23] [expr 127-(0*16)]
			debug write SCC\ SCC [expr $base+24] [expr 128-(1*16)]
			debug write SCC\ SCC [expr $base+25] [expr 128-(2*16)]
			debug write SCC\ SCC [expr $base+26] [expr 128-(3*16)]
			debug write SCC\ SCC [expr $base+27] [expr 128-(4*16)]
			debug write SCC\ SCC [expr $base+28] [expr 128-(5*16)]
			debug write SCC\ SCC [expr $base+29] [expr 128-(6*16)]
			debug write SCC\ SCC [expr $base+30] [expr 128-(7*16)]
			debug write SCC\ SCC [expr $base+31] [expr 128-(8*16)]					
		}
	}	
}


namespace export toggle_psg2scc
namespace export set_scc_wave

}; # namespace

namespace import psg2scc::*
