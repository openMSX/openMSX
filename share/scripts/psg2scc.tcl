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
	set_scc_wave 0 3
	set_scc_wave 1 3
	set_scc_wave 2 3
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
	if {$reg != -1} { debug write "SCC SCC" $reg $val }
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
		debug write "SCC SCC" 0xaf 0
	}
}

proc set_scc_wave {channel form} {
	
	set base [expr $channel*32]

	switch $form {
	#saw tooth
	1 {
		for {set i 0} {$i < 32} {incr i} {
			debug write "SCC SCC" [expr $base+$i] [expr 255-($i*8)]
		}
	}
	
	#square
	2 {
		for {set i 0} {$i < 16} {incr i} {
			debug write "SCC SCC" [expr $base+$i+ 0] 128
			debug write "SCC SCC" [expr $base+$i+16] 127
		}
	}

	#triangle
	3 {
		for {set i 0} {$i < 16} {incr i} {
			set v1 [expr  128 - 16 * $i]
			set v2 [expr -128 + 16 * $i]
			set v1 [expr ($v1 >= 128) ? 127 : $v1]
			debug write "SCC SCC" [expr $base+$i+ 0] [expr $v1 & 255]
			debug write "SCC SCC" [expr $base+$i+16] [expr $v2 & 255]
		}
	}
	}
}

namespace export toggle_psg2scc
namespace export set_scc_wave

}; # namespace

namespace import psg2scc::*
