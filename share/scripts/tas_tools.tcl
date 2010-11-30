namespace eval tas {

### frame counter ###

set_help_text toggle_frame_counter\
{Toggles display of a frame counter in the lower right corner.}

proc toggle_frame_counter {} {
	if [osd exists framecount] {
		osd destroy framecount
		return ""
	}

	osd create rectangle framecount \
		-x 269 -y 223 -h 10 -w 50 -scaled true \
		-rgba "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80" \
		-borderrgba 0x00000040 -bordersize 0.5
	osd create text framecount.text -x 3 -y 2 -size 4 -rgba 0xffffffff
	framecount_update
	return ""
}

proc framecount_update {} {
	if {![osd exists framecount]} return
	osd configure framecount.text -text "Frame: [machine_info VDP_frame_count]"
	after frame [namespace code framecount_update]
}


### frame advance/reverse and helper procs for TAS mode key bindings ###

proc get_frame_time {} {
	return [expr (1368.0 * (([vdpreg 9] & 2) ? 313 : 262)) / (6 * 3579545)]
}

set_help_text advance_frame \
{Emulates until the next frame is generated and then pauses openMSX. Useful to
bind to a key and emulate frame by frame.}

proc advance_frame {} {
	after time [get_frame_time] "set ::pause on"
	set ::pause off
	return ""
}

set_help_text reverse_frame \
{Rewind one frame back in time. Useful to
bind to a key in combination with advance_frame.}

proc reverse_frame {} {
	array set stat [reverse status]
	set t [expr $stat(current) - [get_frame_time]]
	if {$t < 0} { set t 0 }
	reverse goto $t
}

proc load_replay { name } {
	reverse loadreplay -goto savetime $name
	return ""
}


### Very basic replay slot selector ###

user_setting create string current_replay_slot "Name of the current replay slot." slot0

proc list_slots {} {
	set slots [list]
	for {set i 0} {$i <= 9} {incr i} {
		lappend slots "slot$i"
	}
	return $slots
}

proc menu_create_slot_menu {} {
	set items [list_slots]
	set menu_def \
		{ execute tas::set_slot
			font-size 8
			border-size 2
			width 100
			xpos 100
			ypos 100
			header { text "Select Replay Slot"
					font-size 10
					post-spacing 6 }}

	return [osd_menu::prepare_menu_list $items 10 $menu_def]
}

proc set_slot { item } {
	osd_menu::menu_close_all
	set ::current_replay_slot $item
}

proc open_select_slot_menu {} {
	osd_menu::do_menu_open [menu_create_slot_menu]
	osd_menu::select_menu_item $::current_replay_slot
	for {set i 0} {$i < [llength [tas::list_slots]]} {incr i} {
		bind_default "$i" "tas::set_slot [lindex [tas::list_slots] $i]; tas::unbind_number_keys"
	}
}

proc unbind_number_keys {} {
	for {set i 0} {$i < [llength [tas::list_slots]]} {incr i} {
		unbind_default "$i"
	}
}

proc save_replay_slot {} {
	reverse savereplay $::current_replay_slot
}

proc load_replay_slot {} {
	load_replay $::current_replay_slot
}


### Show Cursor Keys / 'fire buttons and others' ###

proc show_keys {} {
	if {![osd exists cursors]} return

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
	osd create rectangle cursors.$name -x $x -y $y -w 16 -h 10 \
	-rgba "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80" \
	-bordersize 0.5 -borderrgba 0x00000040
	osd create text cursors.$name.text -x 2 -y 2 -text $name -size 4 -rgba 0xffffffff
}

proc toggle_cursors {} {
	if [osd exists cursors] {
		osd destroy cursors
	} else {
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
	}
}

### RAM Watch ###

variable addr_watches [list]   ;# sorted list of RAM watch addresses

set_help_text ram_watch_add\
{Add an address (in hex) in RAM to the list of watch addresses on the right side of the screen. The list will be updated in real time, whenever a value changes.}

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
		osd create rectangle ram_watch \
			-x 0 -y 0 -h 240 -w 320 -scaled true -rgba 0x00000000
		osd create rectangle ram_watch.addr \
			-x 288 -y 1 -w 31 -h 221 \
			-rgba "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80" \
			-borderrgba 0x00000040 -bordersize 0.5
		osd create text ram_watch.addr.title -text "Ram Watch" -x 2 -y 2 -size 4 -rgba 0xffffffff
	}

	# add one extra entry
	osd create rectangle ram_watch.addr.mem$i \
		-x 2 -y [expr 8+($i*6)] -h 5 -w 16 -rgba 0x40404080
	osd create text  ram_watch.addr.mem$i.text \
		-size 4 -rgba 0xffffffff
	osd create rectangle ram_watch.addr.val$i \
		-x 19 -y [expr 8+($i*6)] -h 5 -w 10 -rgba 0x40404080
	osd create text  ram_watch.addr.val$i.text \
		-size 4 -rgba 0xffffffff

	ram_watch_update_addresses
	if {$i == 0} {
		ram_watch_update_values
	}
	return ""
}

set_help_text ram_watch_remove\
{Remove an address (in hex) in RAM from the list of watch addresses on the right side of the screen. When the last address is removed, the list will disappear automatically.}

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
