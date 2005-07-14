#
# Returns the selected slot for the given memory page.
# This proc is typically used as a helper for a larger proc.
# 
#  @param page The memory page (0-3)
#  @result Returns a TCL list with two elements.
#          First element is the primary slot (0-3).
#          Second element is the secondary slot (0-3) or 'X'
#          in case this slot was not expanded
#
proc get_selected_slot { page } {
	set ps_reg [debug read "ioports" 0xA8]
	set ps [expr ($ps_reg >> (2 * $page)) & 0x03]
	if [debug read "issubslotted" $ps] {
		set ss_reg [debug read "slotted memory" [expr 0x10000 * $ps + 0xFFFF]]
		set ss [expr (($ss_reg ^ 255) >> (2 * $page)) & 0x03]
	} else {
		set ss "X"
	}
	return [list $ps $ss]
}

#
# Returns a nicely formatted overview of the selected slots
#
proc slotselect { } {
	set result ""
	for { set page 0 } { $page < 4 } { incr page } {
		set tmp [get_selected_slot $page]
		set ps [lindex $tmp 0]
		set ss [lindex $tmp 1]
		append result [format "%04X" [expr 0x4000 * $page]] ": slot " $ps
		if {$ss != "X"} { append result "." $ss }
		append result "\n"
	}
	return $result
}

#
# Test whether the CPU's program counter is inside a certain slot.
# Typically used to set breakpoints in specific slots.
#
proc pc_in_slot { ps {ss "X"} } {
	set d "CPU regs"
	set pc [expr [debug read $d 20] * 256 + [debug read $d 21]]
	set page [expr $pc >> 14]
	set tmp [get_selected_slot $page]
	set pc_ps [lindex $tmp 0]
	set pc_ss [lindex $tmp 1]
	if { $ps != $pc_ps } { return false }
	if [string equal $ss    "X"] { return true }
	if [string equal $pc_ss "X"] { return true }
	return [expr $ss == $pc_ss]
}
