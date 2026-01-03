namespace eval trace {

user_setting create boolean vdpcmdtrace "VDP command tracing on/off" false

proc trace_vdpcmd {} {
	set cmd [debug probe read "VDP.command"]
	if {$cmd eq ""} return
	puts stderr $cmd
}

variable probe_bp_id
proc setting_changed {name1 name2 op} {
	variable probe_bp_id

	if {$::vdpcmdtrace} {;# setting changed from disabled to enabled
		set probe_bp_id [debug probe set_bp "VDP.command" 1 [namespace code trace_vdpcmd]]
	} else { ;# setting changed from enabled to disabled
		catch { debug probe remove_bp $probe_bp_id }
	}
}

trace add variable ::vdpcmdtrace "write" [namespace code setting_changed]
setting_changed ::vdpcmdtrace {} write

}
