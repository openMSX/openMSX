namespace eval vdp_busy {

variable curr_cnt 0
variable last_cnt 0
variable after_id

proc reset {} {
	# each frame copy current counter to last counter and reset current counter
	variable curr_cnt
	variable last_cnt
	variable after_id
	if [info exists after_id] {
		set last_cnt $curr_cnt
		set curr_cnt 0
		after frame vdp_busy::reset
	}
}

proc sample {} {
	# 100 times per frame, sample if VDP command engine is busy
	variable curr_cnt
	variable after_id
	if [info exists after_id] {
		incr curr_cnt [expr {[debug read "VDP status regs" 2] & 1}]
		set fps [expr {([vdpreg 9] & 2) ? 50 : 60}]
		after time [expr {0.01 / $fps}] vdp_busy::sample
	}
}

proc display {} {
	# a couple of times per second update the OSD
	variable last_cnt
	variable after_id
	osd configure "vdp_busy.text" -text "${last_cnt}%"
	set after_id [after time .25 vdp_busy::display]
}

proc toggle_vdp_busy {} {
	variable after_id
	if [info exists after_id] {
		after cancel $after_id
		osd destroy "vdp_busy"
		unset after_id
	} else {
		osd create rectangle "vdp_busy" \
			-x 5 -y 30 -w 42 -h 20 -alpha 0x80
		osd create text "vdp_busy.text" \
			-x 5 -y 3 -rgb 0xffffff
		display
		reset
		sample
	}
	return ""
}

namespace export toggle_vdp_busy

} ;# namespace vdp_busy

namespace import vdp_busy::*
