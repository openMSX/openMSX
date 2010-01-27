#TODO: 
# only show snapshots on timebar when 'debug' parameter is given
#

namespace eval reverse_widgets {
	variable update_after_id
	variable mouse_after_id

	proc toggle_reversebar {} {
		if {[catch {osd info reverse}]} {
			enable_reversebar
		} else {
			disable_reversebar
		}
		return ""
	}

	proc enable_reversebar {} {
		array set stats [reverse status]
		if {$stats(status) == "disabled"} {
			reverse start
			puts stderr "Reverse Auto-Enabled"
		}
		osd create rectangle reverse -scaled true -w 1 -h 1
		osd_widgets::create_power_bar reverse.bar \
			250 3 0x0077ff80 0x00000080 0xffffff80

		update_reversebar
		check_mouse
		
	}

	proc disable_reversebar {} {
		variable update_after_id
		variable mouse_after_id
		after cancel $update_after_id
		after cancel $mouse_after_id
		osd destroy reverse
	}

	proc update_reversebar {} {
		array set stats [reverse status]

		switch $stats(status) {
		"disabled" {
			disable_reversebar
			return
		}
		"replaying" {
			osd configure reverse -fadeTarget 1.0 -fadeCurrent 1.0
		}
		"enabled" {
			foreach {x y} [osd info "reverse.bar" -mousecoord] {}
			if {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1} {
				osd configure reverse -fadePeriod 0.5 -fadeTarget 1.0
			} else {
				osd configure reverse -fadePeriod 5.0 -fadeTarget 0.0
			}
		}}

		set snapshots [split $stats(snapshots)]
		
		if {[llength $snapshots]<2} {
			set update_after_id [after realtime 0.10 [namespace code update_reversebar]]
			return
		}
		
		set totLenght [expr $stats(end) - $stats(begin)]
		set playLength [expr $stats(current) - $stats(begin)]
		set fraction [expr ($totLenght != 0) ? ($playLength / $totLenght) : 0]
		
		set count 0
		foreach snapshot $snapshots {
			set name reverse.bar.snapshot$count
			catch {
				# create new if it doesn't exist yet
				osd create rectangle $name -w 0.5 -h 3 -rgb 0x444444 -z 99
			}
			osd configure $name -relx [expr ($snapshot - $stats(begin)) / $totLenght]
			incr count
		}
		while true {
			# destroy all with higher count number
			if {[catch {osd destroy reverse.bar.snapshot$count}]} {
				break
			}
			incr count
		}

		osd_widgets::update_power_bar reverse.bar \
			33 235 $fraction \
			"[formatTime $playLength] / [formatTime $totLenght]"
		variable update_after_id
		set update_after_id [after realtime 0.10 [namespace code update_reversebar]]
	}

	proc check_mouse {} {
		foreach {x y} [osd info "reverse.bar" -mousecoord] {}
		if {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1} {
			array set stats [reverse status]
			reverse goto [expr $stats(begin) + $x * ($stats(end) - $stats(begin))]
		}
		variable mouse_after_id
		set mouse_after_id [after "mouse button1 down" [namespace code check_mouse]]
	}

	proc formatTime {seconds} {
		format "%02d:%02.f" [expr int($seconds / 60)] [expr fmod($seconds, 60)]
	}

	namespace export toggle_reversebar
}

namespace import reverse_widgets::*
