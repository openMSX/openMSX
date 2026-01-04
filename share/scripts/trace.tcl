namespace eval trace {

# vdpcmdtrace

user_setting create boolean vdpcmdtrace "VDP command tracing on/off" false

proc trace_vdpcmd {} {
	set cmd [debug probe read "VDP.command"]
	if {$cmd eq ""} return
	puts stderr $cmd
}

variable probe_bp_id
proc vdp_setting_changed {name1 name2 op} {
	variable probe_bp_id

	if {$::vdpcmdtrace} {;# setting changed from disabled to enabled
		set probe_bp_id [debug probe set_bp "VDP.command" 1 [namespace code trace_vdpcmd]]
	} else { ;# setting changed from enabled to disabled
		catch { debug probe remove_bp $probe_bp_id }
	}
}

trace add variable ::vdpcmdtrace "write" [namespace code vdp_setting_changed]
vdp_setting_changed ::vdpcmdtrace {} write

# cputrace

user_setting create boolean my_cputrace "CPU tracing on/off" false

# Note: this prints the instruction and register values from *before* that
# instruction has executed. This is different from earlier openMSX version
# (which already showed update register values).
# Both conventions are used in tracing, but the current one is more common.
proc trace_cpu {} {
	# This reads 8 16-bit registers and formats them as hex.
	# The obvious way would be via '[reg nn]' and '[format %04x ..]'.
	# But benchmarking showed this way is significantly faster.
	set hex [binary encode hex [debug read_block "CPU regs" 0 24]]
	set af [string range $hex  0  3]
	set bc [string range $hex  4  7]
	set de [string range $hex  8 11]
	set hl [string range $hex 12 15]
	set ix [string range $hex 32 35]
	set iy [string range $hex 36 39]
	set pc [string range $hex 40 43]
	set sp [string range $hex 44 47]
	set instr [lindex [debug disasm "0x$pc"] 0]
	puts stderr "$pc : $instr AF=$af BC=$bc DE=$de HL=$hl IX=$ix IY=$iy SP=$sp"
}

variable cond_id
proc cpu_setting_changed {name1 name2 op} {
	variable cond_id

	if {$::my_cputrace} {;# setting changed from disabled to enabled
		set cond_id [debug condition create -command [namespace code trace_cpu]]
	} else { ;# setting changed from enabled to disabled
		catch { debug condition remove $cond_id }
	}
}
trace add variable ::my_cputrace "write" [namespace code cpu_setting_changed]
cpu_setting_changed ::my_cputrace {} write

} ;# namespace eval trace
