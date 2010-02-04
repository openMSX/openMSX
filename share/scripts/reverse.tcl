namespace eval reverse {

	# Enable reverse if not yet enabled. As an optimization, also return
	# reverse-status info (so that caller doesn't have to query it again).
	proc auto_enable {} {
		set stat_list [reverse status]
		array set stat $stat_list
		if {$stat(status) == "disabled"} {
			reverse start
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
		if {[llength $revstat(snapshots)] == 0} return

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
		if {[llength $revstat(snapshots)] == 0} return

		set target [expr $revstat(current) + $minimum]

		# search first snapshots that is after target
		set l [llength $revstat(snapshots)]
		set i 0
		while {($i < $l) && ([lindex $revstat(snapshots) $i] < $target)} {
			incr i
		}

		if {$i < $l} {
			reverse goto [lindex $revstat(snapshots) $i]
		}
	}

	# note: you can't use bindings with modifiers like SHIFT, because they
	# will already stop the replay, as they are MSX keys as well
	bind_default PAGEUP "reverse_prev"
	bind_default PAGEDOWN "reverse_next"

	proc after_switch {} {
		if {$::auto_enable_reverse == "on"} {
			auto_enable
		} elseif {$::auto_enable_reverse == "gui"} {
			reverse_widgets::enable_reversebar false
		}
		after machine_switch [namespace code after_switch]
	}


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

	proc enable_reversebar {{visible true}} {
		reverse::auto_enable

		if {![catch {osd info reverse}]} {
			# osd already enabled
			return
		}

		set fade [expr $visible ? 1.0 : 0.0]
		osd create rectangle reverse \
			-scaled true -x 35 -y 233 -w 250 -h 6 \
			-rgba 0x00000080 -fadeCurrent $fade -fadeTarget $fade
		osd create rectangle reverse.top \
			-x -1 -y   -1 -relw 1 -w 2 -h 1 -z 4 -rgba 0xFFFFFFC0
		osd create rectangle reverse.bottom \
			-x -1 -rely 1 -relw 1 -w 2 -h 1 -z 4 -rgba 0xFFFFFFC0
		osd create rectangle reverse.left \
			-x -1         -w 1 -relh 1      -z 4 -rgba 0xFFFFFFC0
		osd create rectangle reverse.right \
			-relx 1       -w 1 -relh 1      -z 4 -rgba 0xFFFFFFC0
		osd create rectangle reverse.bar \
			-relw 0 -relh 1                 -z 0 -rgba 0x0077FF80
		osd create rectangle reverse.end \
			-relx 0 -x -1 -w 2 -relh 1      -z 2 -rgba 0xFF8080C0
		osd create text      reverse.text \
			-x -10 -y 0 -relx 0.5 -size 5   -z 3 -rgba 0xFFFFFFC0

		update_reversebar
		check_mouse
	}

	proc disable_reversebar {} {
		variable update_after_id
		variable mouse_after_id
		catch { after cancel $update_after_id }
		catch { after cancel $mouse_after_id  }
		catch { osd destroy reverse }
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
			set x 2 ; set y 2
			catch { foreach {x y} [osd info "reverse" -mousecoord] {} }
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
			set name reverse.tick$count
			catch {
				# create new if it doesn't exist yet
				osd create rectangle $name -w 0.5 -relh 1 -rgb 0x444444 -z 1
			}
			osd configure $name -relx [expr ($snapshot - $stats(begin)) * $reciprocalLength]
			incr count
		}
		while true {
			# destroy all with higher count number
			if {[catch {osd destroy reverse.tick$count}]} {
				break
			}
			incr count
		}

		osd configure reverse.bar -relw $fraction
		osd configure reverse.end -relx $fraction
		osd configure reverse.text \
			-text "[formatTime $playLength] / [formatTime $totLenght]"
		variable update_after_id
		set update_after_id [after realtime 0.10 [namespace code update_reversebar]]
	}

	proc check_mouse {} {
		set x 2 ; set y 2
		catch { foreach {x y} [osd info "reverse" -mousecoord] {} }
		if {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1} {
			array set stats [reverse status]
			reverse goto [expr $stats(begin) + $x * ($stats(end) - $stats(begin))]
		}
		variable mouse_after_id
		set mouse_after_id [after "mouse button1 down" [namespace code check_mouse]]
	}

	proc formatTime {seconds} {
		format "%02d:%02d" [expr int($seconds / 60)] [expr int($seconds) % 60]
	}

	namespace export toggle_reversebar
}

namespace import reverse::*
namespace import reverse_widgets::*

user_setting create string "auto_enable_reverse" \
{Controls whether the reverse feature is automatically enabled on startup.
Internally the reverse feature takes regular snapshots of the MSX state,
this has a (small) cost in memory and in performance. On small systems you
don't want this cost, so we don't enable the reverse feature by default.
Possible values for this setting:
  off   Reverse not enabled on startup
  on    Reverse enabled on startup
  gui   Reverse + reverse_bar enabled (see 'help toggle_reversebar')
} off
reverse::after_switch
