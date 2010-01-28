namespace eval reverse {

	# Enable reverse if not yet enabled. As an optimization, also return
	# reverse-status info (so that caller doesn't have to query it again).
	proc auto_enable {} {
		set stat_list [reverse status]
		array set stat $stat_list
		if {$stat(status) == "disabled"} {
			reverse start
			puts stderr "Reverse Auto-Enabled"
		}
		return $stat_list
	}

	set_help_text reverse_prev \
{Go back in time to the previous 'snapshot'.
A 'snapshot' is actually an internal implementation detail, but the\
important thing for this command is that the further back in the past,\
the less dense the snapshots are. So executing this command multiple times\
will take successively bigger steps in the past. Going back to a snapshot\
is also slightly faster than going back to an arbitrary point in time\
(let's say going back a fixed amount of time).
}
	proc reverse_prev {{minimum 1}} {
		array set revstat [auto_enable]

		set target [expr $revstat(current) - $minimum]

		# search latest snapshots that is still before target
		set i [expr [llength $revstat(snapshots)] - 1]
		while {([lindex $revstat(snapshots) $i] > $target) && ($i > 0)} {
			incr i -1
		}

		reverse goto [lindex $revstat(snapshots) $i]
	}

	set_help_text reverse_next \
{This is very much like 'reverse_prev', but instead it goes to the closest\
snapshot in the future (if possible).
}
	proc reverse_next {{minimum 0}} {
		array set revstat [auto_enable]
		lappend $revstat(snapshots) $revstat(end)

		set target [expr $revstat(current) + $minimum]

		# search first snapshots that is after target
		set l [llength $revstat(snapshots)]
		set i 0
		while {([lindex $revstat(snapshots) $i] < $target) && ($i < $l)} {
			incr i
		}

		reverse goto [lindex $revstat(snapshots) $i]
	}

	bind_default F6 "reverse_prev"
	# Note: I thought about using SHIFT+F6 for reverse_next, but pressing
	#       SHIFT already forces a stop replay.

	namespace export reverse_prev
	namespace export reverse_next
}

namespace eval reverse_widgets {
	variable update_after_id
	variable mouse_after_id

	set_help_text toggle_reversebar \
{Enable/disable an on-screen reverse bar.
This will show the recorded 'reverse history' and the current position in\
this history. It is possible to click on this bar to immediately jump to a\
specific point in time. If the current position is at the end of the history\
(i.e. we're not replaying an existing history), this reverse bar will slowly\
fade out. You can make it reappear by moving the mouse over it.
}
	proc toggle_reversebar {} {
		if {[catch {osd info reverse}]} {
			enable_reversebar
		} else {
			disable_reversebar
		}
		return ""
	}

	proc enable_reversebar {} {
		reverse::auto_enable
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

		set snapshots $stats(snapshots)
		set totLenght [expr $stats(end) - $stats(begin)]
		set playLength [expr $stats(current) - $stats(begin)]
		set reciprocalLength [expr ($totLenght != 0) ? (1.0 / $totLenght) : 0]
		set fraction [expr $playLength * $reciprocalLength]

		set count 0
		foreach snapshot $snapshots {
			set name reverse.bar.snapshot$count
			catch {
				# create new if it doesn't exist yet
				osd create rectangle $name -w 0.5 -h 3 -rgb 0x444444 -z 99
			}
			osd configure $name -relx [expr ($snapshot - $stats(begin)) * $reciprocalLength]
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

namespace import reverse::*
namespace import reverse_widgets::*
