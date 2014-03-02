set_help_text toggle_freq "Switch the VDP between PAL (50 Hz)/NTSC (60 Hz)."

proc toggle_freq {} {
	debug write "VDP regs" 9    [expr {[debug read "VDP regs" 9]    ^ 2}]
	debug write "memory" 0xFFE8 [expr {[debug read "memory" 0xFFE8] ^ 2}]

	set freq 60
	if {[expr {[debug read "VDP regs" 9] & 0x2}]} {
		set freq 50
	}
	osd::display_message "Frequency set to $freq Hz" info
}
