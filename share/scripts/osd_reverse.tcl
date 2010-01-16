namespace eval reverse_widgets {

	variable update_after_id
	variable mouse_after_id

	proc toggle_reversebar {} {
		variable update_after_id
		variable mouse_after_id
		if {[catch {osd info reverse.bar -rgba} errmsg]} {
			namespace import ::osd_widgets::*
			# TODO: get rid of MSX coordinates dependency!
			osd create rectangle reverse -scaled true -w 1 -h 1
			create_power_bar reverse.bar 250 3 0x0077ff80 0x00000080 0xffffff80
			update_reversebar
			set_mouse_trigger
			return
		}
		after cancel $update_after_id
		after cancel $mouse_after_id
		osd destroy reverse
	}
	
	proc update_reversebar {} {

		variable update_after_id

		array set stats [reverse status]
		if {$stats(status) == "disabled"} {
			reverse start
			puts stderr "Reverse Auto-Enabled"
		} else {
		
			set totLenght [expr $stats(end) - $stats(begin)]
			set playLength [expr $stats(current) - $stats(begin)]
			set fraction [expr ($playLength / $totLenght)]
			
			set playTime [reverse_widgets::maketime ($playLength)]
			set totTime [reverse_widgets::maketime ($totLenght)]
			#puts [expr $playLength/$totLenght]
			update_power_bar reverse.bar \
				33\
				235 \
				$fraction \
				[format "$playTime / $totTime (%0.2f%%)" [expr $fraction * 100]]
		}
		set update_after_id [after realtime 0.10 [namespace code update_reversebar]]
	}

	proc set_mouse_trigger {} {
		variable mouse_after_id
		set mouse_after_id [after "mouse button1 down" {
			foreach {x y} [osd info "reverse.bar" -mousecoord] {}
			if {$x >= 0 && $x <= 1 && $y >= 0 && $y <= 1} {
				array set stats [reverse status]
				reverse goto [expr $stats(begin) + $x * ($stats(end) - $stats(begin))]
			}
			reverse_widgets::set_mouse_trigger
		}]
	}
	
	proc maketime {seconds} {
		set seconds [expr int($seconds)]
		return  [format "%02d:%02d" [expr $seconds/60] [expr $seconds%60]]
	}

	namespace export toggle_reversebar
	namespace export maketime
}

namespace import reverse_widgets::*
