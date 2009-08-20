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
	set_scc_wave 1 2
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

proc set_scc_form {channel wave} {
	set base [expr $channel*32]

	for {set i 0} {$i < 32} {incr i} {	
		debug write "SCC SCC" [expr $base+$i] "0x[string range $wave [expr $i*2] [expr $i*2]+1]"
	}
}

proc set_scc_wave {channel form} {
	
	set base [expr $channel*32]

	switch $form {

	0 {
		#Saw Tooth
		for {set i 0} {$i < 32} {incr i} {
			debug write "SCC SCC" [expr $base+$i] [expr 255-($i*8)]
		}
	}

	1 {
		#Square
		set_scc_form $channel "7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f80808080808080808080808080808080"
	}

	2 {	#Triangle
		for {set i 0} {$i < 16} {incr i} {
			set v1 [expr  128 - 16 * $i]
			set v2 [expr -128 + 16 * $i]
			set v1 [expr ($v1 >= 128) ? 127 : $v1]
			debug write "SCC SCC" [expr $base+$i+ 0] [expr $v1 & 255]
			debug write "SCC SCC" [expr $base+$i+16] [expr $v2 & 255]
 		}
	}

	3 {
		#Sin Wave
		set_scc_form $channel "001931475A6A757D7F7D756A5A47311900E7CFB9A6968B8380838B96A6B9CFE7"
	}
	
	4 {
		#Organ
		set_scc_form $channel "0070502050703000507F6010304000B0106000E0F000B090C010E0A0C0F0C0A0"
	}
	
	5 {
		#SAWWY001
		set_scc_form $channel "636E707070705F2198858080808086AB40706F8C879552707052988080808EC1"
	}
	
	6 {
		
		#SQROOT01
		set_scc_form $channel "00407F401001EAD6C3B9AFA49C958F8A86838183868A8F959CA4AFB9C3D6EAFF"
	}
	
	7 {
		#SQROOT01
		set_scc_form $channel "636E707070705F2198858080808086AB40706F8C879552707052988080808EC1"
	}
	
	8 {
		#DYERVC01
		set_scc_form $channel "00407F4001C081C001407F4001C0014001E0012001F0011001FFFFFFFF404040"
	}
	}
}

namespace export toggle_psg2scc
namespace export set_scc_wave

}; # namespace

namespace import psg2scc::*