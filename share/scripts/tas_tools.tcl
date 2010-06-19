namespace eval tas {

set_help_text toggle_frame_counter\
{Toggles display of a frame counter in the lower right corner.

Note that the absolute value of the frame counter will be wrong after a
PAL/NTSC switch (50Hz/60Hz switch). Relative values are correct, so this
can be used for example to advance N frames. We will fix the absolute
value after openMSX 0.8.1 is released. }

proc toggle_frame_counter {} {
	if {![catch {osd info framecount}]} {
		osd destroy framecount
		return ""
	}

	osd_widgets::box framecount -x 269 -y 224 -h 8 -w 50 -rgba 0x80808080 -fill "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80" -scaled true
	osd create text framecount.text -x 3 -y 2 -size 4 -rgba 0xffffffff
	framecount_update
	return ""
}

proc get_frame_time {} {
	return [expr (1368.0 * (([vdpreg 9] & 2) ? 313 : 262)) / (6 * 3579545)]
}

proc framecount_current {} {
	return [expr int([machine_info time] / [get_frame_time])]
}

proc framecount_update {} {
	if {[catch {osd info framecount}]} return
	osd configure framecount.text -text "Frame: [framecount_current]"
	after frame [namespace code framecount_update]
}


set_help_text advance_frame \
{Emulates until the next frame is generated and then pauses openMSX. Useful to
bind to a key and emulate frame by frame.}

proc advance_frame {} {
	after time [get_frame_time] "set ::pause on"
	set ::pause off
	return ""
}

set_help_text advance_frame \
{Rewind one frame back in time. Useful to
bind to a key in combination with advance_frame.}

proc reverse_frame {} {
	array set stat [reverse status]
	goto_time [expr $stat(current) - [get_frame_time]]
}

variable doing_black_screen_reverse false

proc correct_black_screen_for_time { t } {
	variable doing_black_screen_reverse

	# after loading a savestate, it takes two(!) frames before the
	# rendered image is correct. (TODO investigate why it needs
	# two, I only expected one). As a workaround we go back two
	# frames and replay those two frames.

	# duration of two video frames
	set t_cor [expr {2 * [get_frame_time] }]
	reverse goto [expr $t - $t_cor]
	set ::pause off
	set doing_black_screen_reverse true
	after time $t_cor { set ::pause on; set tas::doing_black_screen_reverse false }
}

proc goto_time { t } {
	variable doing_black_screen_reverse
	# ignore goto command if we're still doing the black screen correction
	if {!$doing_black_screen_reverse} {
		if {$::pause} {
			correct_black_screen_for_time $t
		} else {
			reverse goto $t
		}
	}
}

proc load_replay { name } {
	reverse loadreplay $name
	array set stat [reverse status]
	goto_time $stat(end)
	return ""
}

proc go_back_one_second {} {
	array set stat [reverse status]
	goto_time [expr $stat(current) - 1]
}

set_help_text enable_tas_mode \
{Enables the highly experimental TAS mode, giving you 8 savestate slots, to be
used with F1-F8 (load) and SHIFT-F1 (to F8) to save. It actually saves
replays. After loading, openMSX will advance to the end of the replay
and pause. This will go slower and slower if you have longer replays.
(Will be improved after openMSX 0.8.0 is released.)

It will also enable the frame counter and give you a frame_advance key binding
under the END key.
}

proc enable_tas_mode {} {
	# assume frame counter is disabled here
	toggle_frame_counter
	toggle_cursors
	reverse_widgets::enable_reversebar false

	# set up the quicksave/load "slots"
	set basename "quicksave"
	for {set i 1} {$i <= 8} {incr i} {
		bind_default "F$i" tas::load_replay "$basename$i"
		bind_default "SHIFT+F$i" "reverse savereplay $basename$i"
	}

	# set up frame advance/reverse
	bind_default END -repeat advance_frame
	bind_default HOME -repeat reverse_frame

	# set up special reverse for PgUp, which always goes back 1 second and correct for
	# the pause artifact
	bind_default PAGEUP "tas::go_back_one_second"

	return "WARNING 1: TAS mode is still very experimental and will almost certainly change next release!"
}

# -- Show Cursor Keys / 'fire buttons and others'
proc show_keys {} {
	if {[catch {osd info cursors}]} return

	show_key_press right [is_key_pressed 8 7]
	show_key_press down  [is_key_pressed 8 6]
	show_key_press up    [is_key_pressed 8 5]
	show_key_press left  [is_key_pressed 8 4]
	show_key_press space [is_key_pressed 8 0]

	show_key_press m     [is_key_pressed 4 2]
	show_key_press n     [is_key_pressed 4 3]
	show_key_press z     [is_key_pressed 5 7]
	show_key_press x     [is_key_pressed 5 5]

	show_key_press graph [is_key_pressed 6 2]
	show_key_press ctrl  [is_key_pressed 6 1]
	show_key_press shift [is_key_pressed 6 0]
	
	after realtime 0.1 [namespace code show_keys]
}

#move to other TCL script?
proc is_key_pressed {row bit} {
	return [expr !([debug read keymatrix $row] & (1 << $bit))]
}

proc show_key_press {key state} {
	set keycol [expr {$state ? 0xff000080 : "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80"}]
	osd configure cursors.$key -rgba $keycol
}

proc create_key {name x y} {
	osd_widgets::box cursors.$name -x $x -y $y -w 16 -h 10 -rgba "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80"
	osd create text cursors.$name.text -x 2 -y 2 -text $name -size 4 -rgba 0xffffffff
}

proc toggle_cursors {} {
	if {[catch {osd info cursors}]} {
		osd create rectangle cursors -x 64 -y 215 -h 26 -w 204 -scaled true -rgba 0x00000000
		#cursor keys
		create_key up 20 2
		create_key left 2 8
		create_key down 20 14
		create_key right 38 8
		#fire buttons and others
		create_key space 60 8
		create_key m 78 8
		create_key n 96 8
		create_key z 114 8
		create_key x 132 8
		create_key graph 150 8
		create_key ctrl 168 8
		create_key shift 186 8

		show_keys
	} else {
		osd destroy cursors
	}
}

# -- RAM Watch
variable addr_watches [list]   ;# sorted list of RAM watch addresses

proc ram_watch_add {addr_str} {
	variable addr_watches

	# sanitize input
	set addr [expr int($addr_str)]
	if {($addr < 0) || ($addr > 0xffff)} {
		error "Please use a 16 bit address."
	}

	# check for duplicates
	#puts stderr "| $addr_watches |"
	#puts stderr "| [lsearch $addr_watches $addr] |"
	if {[lsearch $addr_watches $addr] != -1} {
		error "Address [format 0x%04X $addr] already being watched."
	}

	# add address to list
	set i [llength $addr_watches]
	lappend addr_watches $addr
	set addr_watches [lsort -integer $addr_watches]

	# if OSD doesn't exist yet create it
	if {$i == 0} {
		osd create rectangle ram_watch -x 0 -y 0 -h 240 -w 320 -scaled true -alpha 0
		osd_widgets::box ram_watch.addr -x 288 -y 1 -w 31 -h 221 -rgba 0x80808080 -fill "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80"
		osd create text ram_watch.addr.title -text "Ram Watch" -x 2 -y 2 -size 4 -rgba 0xffffffff
	}

	# add one extra entry
	osd create rectangle ram_watch.addr.mem$i -x 2 -y [expr 8+($i*6)] -h 5 -w 16 -rgba 0x40404080
	osd create text  ram_watch.addr.mem$i.text -size 4 -rgba 0xffffffff
	osd create rectangle ram_watch.addr.val$i -x 19 -y [expr 8+($i*6)] -h 5 -w 10 -rgba 0x40404080
	osd create text  ram_watch.addr.val$i.text -size 4 -rgba 0xffffffff

	ram_watch_update_addresses
	if {$i == 0} {
		ram_watch_update_values
	}
	return ""
}

proc ram_watch_remove {addr_str} {
	variable addr_watches

	# sanitize input
	set addr [expr int($addr_str)]
	if {($addr < 0) || ($addr > 0xffff)} {
		error "Please use a 16 bit address."
	}

	# check watch exists
	set index [lsearch $addr_watches $addr]
	if {$index == -1} {
		error "Address [format 0x%04X $addr] was not being watched."
	}

	#remove address from list
	set addr_watches [lreplace $addr_watches $index $index]
	set i [llength $addr_watches]

	#remove one OSD entry
	osd destroy ram_watch.addr.mem$i
	osd destroy ram_watch.addr.val$i

	#if all elements are gone don't display anything anymore.
	if {$i == 0} {
		osd destroy ram_watch
	} else {
		ram_watch_update_addresses
	}
	return ""
}

proc ram_watch_update_addresses {} {
	variable addr_watches

	set i 0
	foreach addr $addr_watches {
		osd configure ram_watch.addr.mem$i.text -text [format 0x%04X $addr]
		incr i
	}
}

proc ram_watch_update_values {} {
	variable addr_watches
	if {[llength $addr_watches] == 0} return

	set i 0
	foreach addr $addr_watches {
		osd configure ram_watch.addr.val$i.text -text [format 0x%02X [peek $addr]]
		incr i
	}
	after frame [namespace code ram_watch_update_values]
}


namespace export toggle_frame_counter
namespace export advance_frame
namespace export reverse_frame
namespace export enable_tas_mode
namespace export ram_watch_add
namespace export ram_watch_remove
namespace export toggle_cursors
}

namespace import tas::*
