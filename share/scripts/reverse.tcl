namespace eval reverse {

variable is_dingoo [string match *-dingux* $::tcl_platform(osVersion)]
variable default_auto_enable_reverse
if {$is_dingoo} {
	set default_auto_enable_reverse "off"
} else {
	set default_auto_enable_reverse "gui"
}

# Enable reverse if not yet enabled. As an optimization, also return
# reverse-status info (so that caller doesn't have to query it again).
proc auto_enable {} {
	set stat_dict [reverse status]
	if {[dict get $stat_dict status] eq "disabled"} {
		reverse start
	}
	return $stat_dict
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
proc reverse_prev {{minimum 1} {maximum 15}} {
	set stats [auto_enable]
	set snapshots [dict get $stats snapshots]
	set num_snapshots [llength $snapshots]
	if {$num_snapshots == 0} return

	set current [dict get $stats current]
	set upperTarget [expr {$current - $minimum}]
	set lowerTarget [expr {$current - $maximum}]

	# search latest snapshot that is still before upperTarget
	set i [expr {$num_snapshots - 1}]
	while {([lindex $snapshots $i] > $upperTarget) && ($i > 0)} {
		incr i -1
	}
	# but don't go below lowerTarget
	set t [lindex $snapshots $i]
	if {$t < $lowerTarget} {set t $lowerTarget}

	reverse goto $t
}

set_help_text reverse_next \
{This is very much like 'reverse_prev', but instead it goes to the closest\
snapshot in the future (if possible).
}
proc reverse_next {{minimum 0} {maximum 15}} {
	set stats [auto_enable]
	set snapshots [dict get $stats snapshots]
	set num_snapshots [llength $snapshots]
	if {$num_snapshots == 0} return

	set current [dict get $stats current]
	set lowerTarget [expr {$current + $minimum}]
	set upperTarget [expr {$current + $maximum}]

	# search first snapshot that is after lowerTarget
	lappend snapshots [dict get $stats end]
	set i 0
	while {($i < $num_snapshots) && ([lindex $snapshots $i] < $lowerTarget)} {
		incr i
	}

	if {$i < $num_snapshots} {
		# but don't go above upperTarget
		set t [lindex $snapshots $i]
		if {$t > $upperTarget} {set t $upperTarget}
		reverse goto $t
	}
}

proc goto_time_delta {delta} {
	set t [expr {[dict get [reverse status] current] + $delta}]
	if {$t < 0} {set t 0}
	reverse goto $t
}

proc go_back_one_step {} {
	goto_time_delta [expr {-$::speed / 100.0}]
}

proc go_forward_one_step {} {
	goto_time_delta [expr { $::speed / 100.0}]
}

# note: you can't use bindings with modifiers like SHIFT, because they
# will already stop the replay, as they are MSX keys as well
bind_default PAGEUP   -repeat "reverse::go_back_one_step"
bind_default PAGEDOWN -repeat "reverse::go_forward_one_step"

proc after_switch {} {
	# enabling reverse could fail if the active machine is an 'empty'
	# machine (e.g. because the last machine is removed or because
	# you explictly switch to an empty machine)
	catch {
		if {$::auto_enable_reverse eq "on"} {
			auto_enable
		} elseif {$::auto_enable_reverse eq "gui"} {
			reverse_widgets::enable_reversebar false
		}
	}
	after machine_switch [namespace code after_switch]
}

namespace export reverse_prev
namespace export reverse_next
namespace export goto_time_delta

} ;# namespace reverse


namespace eval reverse_widgets {

variable reverse_bar_update_interval 0.10

variable update_after_id 0
variable mouse_after_id 0
variable overlay_counter
variable prev_x 0
variable prev_y 0
variable overlayOffset

set_help_text toggle_reversebar \
{Enable/disable an on-screen reverse bar.
This will show the recorded 'reverse history' and the current position in\
this history. It is possible to click on this bar to immediately jump to a\
specific point in time. If the current position is at the end of the history\
(i.e. we're not replaying an existing history), this reverse bar will slowly\
fade out. You can make it reappear by moving the mouse over it.
}
proc toggle_reversebar {} {
	if {[osd exists reverse]} {
		disable_reversebar
	} else {
		enable_reversebar
	}
	return ""
}

proc enable_reversebar {{visible true}} {
	reverse::auto_enable

	if {[osd exists reverse]} {
		# osd already enabled
		return
	}

	# Hack: put the reverse bar at the top/bottom when the icons
	#  are at the bottom/top. It would be better to have a proper
	#  layout manager for the OSD stuff.
	if {[catch {set led_y [osd info osd_icons -y]}]} {
		set led_y 0
	}
	set y [expr {($led_y == 0) ? 231 : 1}]
	# Set time indicator position (depending on reverse bar position)
	variable overlayOffset [expr {($led_y > 16) ? 1.1 : -1.25}]

	# Reversebar
	set fade [expr {$visible ? 1.0 : 0.0}]
	osd create rectangle reverse \
		-scaled true -x 34 -y $y -w 252 -h 8 \
		-rgba 0x00000080 -fadeCurrent $fade -fadeTarget $fade \
		-borderrgba 0xFFFFFFC0 -bordersize 1
	osd create rectangle reverse.int \
		-x 1 -y 1 -w 250 -h 6 -rgba 0x00000000 -clip true
	osd create rectangle reverse.int.bar \
		-relw 0 -relh 1 -z 3 -rgba "0x0044aa80 0x2266dd80 0x0055cc80 0x55eeff80"
	osd create rectangle reverse.int.end \
		-relx 0 -x -1 -w 2 -relh 1      -z 3 -rgba 0xff8000c0
	osd create text      reverse.int.text \
		-x -10 -y 0 -relx 0.5 -size 5   -z 6 -rgba 0xffffffff

	# on mouse over hover box
	osd create rectangle reverse.mousetime \
		-relx 0.5 -rely 1 -relh 0.75 -z 4 \
		-rgba "0xffdd55e8 0xddbb33e8 0xccaa22e8 0xffdd55e8" \
		-bordersize 0.5 -borderrgba 0xffff4480
	osd create text reverse.mousetime.text \
		-size 5 -z 4 -rgba 0x000000ff

	update_reversebar

	variable mouse_after_id
	set mouse_after_id [after "mouse button1 down" [namespace code check_mouse]]
}

proc disable_reversebar {} {
	variable update_after_id
	variable mouse_after_id
	after cancel $update_after_id
	after cancel $mouse_after_id
	osd destroy reverse
}

proc update_reversebar {} {
	catch {update_reversebar2}
	variable reverse_bar_update_interval
	variable update_after_id
	set update_after_id [after realtime $reverse_bar_update_interval [namespace code update_reversebar]]
}

proc update_reversebar2 {} {
	set stats [reverse status]

	set x 2; set y 2
	catch {lassign [osd info "reverse.int" -mousecoord] x y}
	set mouseInside [expr {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1}]

	switch [dict get $stats status] {
		"disabled" {
			disable_reversebar
			return
		}
		"replaying" {
			osd configure reverse -fadeTarget 1.0 -fadeCurrent 1.0
			osd configure reverse.int.bar \
				-rgba "0x0044aaa0 0x2266dda0 0x0055cca0 0x55eeffa0"
		}
		"enabled" {
			osd configure reverse.int.bar \
				-rgba "0xff4400a0 0xdd3300a0 0xbb2200a0 0xcccc11a0"
			if {$mouseInside} {
				osd configure reverse -fadePeriod 0.5 -fadeTarget 1.0
			} else {
				osd configure reverse -fadePeriod 5.0 -fadeTarget 0.0
			}
		}
	}

	set snapshots [dict get $stats snapshots]
	set begin     [dict get $stats begin]
	set end       [dict get $stats end]
	set current   [dict get $stats current]
	set totLenght  [expr {$end     - $begin}]
	set playLength [expr {$current - $begin}]
	set reciprocalLength [expr {($totLenght != 0) ? (1.0 / $totLenght) : 0}]
	set fraction [expr {$playLength * $reciprocalLength}]

	# Check if cursor moved compared to previous update,
	# if so reset counter (see below)
	variable overlay_counter
	variable prev_x
	variable prev_y
	if {$prev_x != $x || $prev_y != $y} {
		set overlay_counter 0
		set prev_x $x
		set prev_y $y
	}

	# Display mouse-over time jump-indicator
	# Hide when mouse hasn't moved for some time
	if {$mouseInside && $overlay_counter < 8} {
		variable overlayOffset
		set mousetext [formatTime [expr {$x * $totLenght}]]
		osd configure reverse.mousetime.text -text $mousetext -relx 0.05
		set textsize [lindex [osd info reverse.mousetime.text -query-size] 0]
		osd configure reverse.mousetime -rely $overlayOffset -relx [expr {$x - 0.05}] -w [expr {1.1 * $textsize}]
		incr overlay_counter
	} else {
		osd configure reverse.mousetime -rely -100
	}

	set count 0
	foreach snapshot $snapshots {
		set name reverse.int.tick$count
		if {![osd exists $name]} {
			# create new if it doesn't exist yet
			osd create rectangle $name -w 0.5 -relh 1 -rgba 0x444444ff -z 2
		}
		osd configure $name -relx [expr {($snapshot - $begin) * $reciprocalLength}]
		incr count
	}
	# destroy all with higher count number
	while {[osd destroy reverse.int.tick$count]} {
		incr count
	}

	# Round fraction to avoid excessive redraws caused by rounding errors
	set fraction [expr {round($fraction * 10000) / 10000.0}]
	osd configure reverse.int.bar -relw $fraction
	osd configure reverse.int.end -relx $fraction
	osd configure reverse.int.text \
		-text "[formatTime $playLength] / [formatTime $totLenght]"
}

proc check_mouse {} {
	catch {
		set x 2; set y 2
		catch {lassign [osd info "reverse.int" -mousecoord] x y}
		if {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1} {
			set stats [reverse status]
			set begin [dict get $stats begin]
			set end   [dict get $stats end]
			reverse goto [expr {$begin + $x * ($end - $begin)}]
		}
	}
	variable mouse_after_id
	set mouse_after_id [after "mouse button1 down" [namespace code check_mouse]]
}

proc formatTime {seconds} {
	format "%02d:%02d" [expr {int($seconds / 60)}] [expr {int($seconds) % 60}]
}

namespace export toggle_reversebar

} ;# namespace reverse_widgets

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
} $reverse::default_auto_enable_reverse


# TODO hack:
# The order in which the startup scripts are executed is not defined. But this
# needs to run after the load_icons script has been executed.
#
# It also needs to run after 'after boot' proc in the autoplug.tcl script. The
# reason is tricky:
#  - If this runs before autoplug, then the initial snapshot (initial snapshot
#    triggered by enabling reverse) does not contain the plugged cassette player
#    yet.
#  - Shortly after the autoplug proc runs, this will plug the cassetteplayer.
#    This plug command will be recorded in the event log. This is fine.
#  - The problem is that we don't serialize information for unplugged devices
#    ((only) in the initial snapshot, the cassetteplayer is not yet plugged). So
#    the information about the inserted tape is lost.

# As a solution/hack, we trigger this script at emutime=0. This is after the
# autoplug script (which triggers at 'after boot'. But also this is also
# triggered when openmsx is started via a replay file from the command line
# (in such case 'after boot' is skipped).

after time 0 {reverse::after_switch}
