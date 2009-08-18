namespace eval osd_keyboard {

# KNOWN ISSUES/TODO:
# * "-" key doesn't work (needs escaping) shouldn't use the keymatrix command,
#   but the 'type' command for all keys that are not on the same position in the matrix
#   for all machines
# * lots more? :P

#init vars
variable mouse1_pressed false
variable key_pressed 0
variable row_starts

#init colors
variable key_color 0xffffffc0
variable key_pressed_color 0xff8800ff
variable key_background_color 0x88888880
variable key_hold_color 0x00ff88ff

# Keyboard layout constants.
variable key_height 16
variable key_hspace 2
variable key_vspace 2
variable board_hborder 4
variable board_vborder 4

proc toggle_osd_keyboard {} {

	#If exists destory/reset and exit
	if {![catch {osd info kb -rgba} errmsg]} {
		#destroy virtual keyboard
		osd destroy kb
		#unbind mouse buttons
		unbind_default "mouse button1 down"
		unbind_default "mouse button1 up"
		unbind_default "mouse button3 down"
		#reset keyboard matrix
		for {set i 0} {$i <= 8} {incr i} {
			keymatrixup $i 255
		}
		return ""
	}

	variable mouse1_pressed false
	variable row_starts [list]
	variable key_color
	variable key_background_color

	#bind stuff
	bind_default "mouse button1 down"  {osd_keyboard::key_handler true}
	bind_default "mouse button1 up"    {osd_keyboard::key_handler false}

	bind_default "mouse button3 down"  {osd_keyboard::key_hold_toggle}

	#Define Keyboard (how do we handle the shift/ctrl/graph command?)
	set key_basewidth 18
	set rows {"f-1*28|f-2*28|f-3*28|f-4*28|f-5*28|null*48|select|stop|home|ins|del" \
			 "esc|1|2|3|4|5|6|7|8|9|0|-|=|\\|bs" \
			 "tab*28|Q|W|E|R|T|Y|U|I|O|P|\[|]|return*28" \
			 "ctrl*32|A|S|D|F|G|H|J|K|L|;|'|`|<--*24" \
			 "shift*40|Z|X|C|V|B|N|M|,|.|/|dead|shift*36" \
			 "null*40|caps|graph|space*158|code"}

	# Keyboard layout constants.
	variable key_height
	variable key_hspace
	variable key_vspace
	variable board_hborder
	variable board_vborder

	# Create widgets.
	set board_width \
		[expr 15 * $key_basewidth + 14 * $key_hspace + 2 * $board_hborder]
	set board_height \
		[expr 6 * $key_height + 5 * $key_vspace + 2 * $board_vborder]
	osd create rectangle kb \
		-x [expr (320 - $board_width) / 2 ] \
		-y 4 \
		-w $board_width \
		-h $board_height -scaled true -rgba $key_background_color
	set keycount 0
	for {set y 0} {$y <= [llength $rows]} {incr y} {
		lappend row_starts $keycount
		set x $board_hborder
		foreach {keys} [split [lindex $rows $y]  "|"] {
			set key [split $keys "*"]
			set key_text [lindex $key 0]
			set key_width [lindex $key 1]
			if {$key_width < 1} {set key_width $key_basewidth}

			if {$key_text != "null"} {
				osd create rectangle kb.$keycount \
					-x $x \
					-y [expr $board_vborder + $y * ($key_height + $key_vspace)] \
					-w $key_width \
					-h $key_height \
					-rgba $key_color

				if {$key_text == "<--"} {
					# Merge bottom part of "return" key with top part.
					osd configure kb.$keycount \
						-y [expr [osd info kb.$keycount -y] - $key_vspace] \
						-h [expr [osd info kb.$keycount -h] + $key_vspace]
				}
				osd create text kb.$keycount.text \
					-x 0.1 \
					-y 0.1 \
					-text $key_text \
					-size 8

				incr keycount
			}

			set x [expr $x + $key_width + $key_hspace]
		}
	}

	return ""
}

proc key_at_coord {x y} {
	variable key_height
	variable key_hspace
	variable key_vspace
	variable board_vborder
	variable row_starts
	set row [expr int(floor( \
		($y - $board_vborder + $key_vspace / 2) / ($key_height + $key_vspace) \
		))]
	if {$row >= 0 && $row < [llength $row_starts] - 1} {
		set row_start [lindex $row_starts $row]
		set row_end [lindex $row_starts [expr $row + 1]]
		for {set key_id $row_start} {$key_id < $row_end} {incr key_id} {
			set relx [expr $x - [osd info kb.$key_id -x] + $key_hspace / 2]
			if {$relx >= 0 && $relx < [osd info kb.$key_id -w] + $key_hspace} {
				return $key_id
			}
		}
	}
	return -1
}

proc key_at_mouse {} {
	foreach {x y} [osd info kb -mousecoord] {
		return [key_at_coord \
			[expr $x * [osd info kb -w]] \
			[expr $y * [osd info kb -h]] \
			]
	}
}

proc key_hold_toggle {} {
	variable key_color
	variable key_hold_color

	set key_id [key_at_mouse]
	if {$key_id >= 0} {
		if {[osd info kb.$key_id -rgba] == -64} {
			key_matrix $key_id down
			osd configure kb.$key_id -rgba $key_hold_color
		} else {
			key_matrix $key_id up
			osd configure kb.$key_id -rgba $key_color
		}
	}
}

proc key_handler {mouse_state} {
	variable key_pressed

	variable key_pressed_color
	variable key_color

	#scan which key is down (can be optimized but for now it's ok)
	if {$mouse_state} {
		set key_id [key_at_mouse]
		if {$key_id >= 0} {
			osd configure kb.$key_id -rgba $key_pressed_color
			key_matrix $key_id down
			set key_pressed $key_id
		}
	} else {
		osd configure kb.$key_pressed -rgba $key_color
		key_matrix $key_pressed up
	}
}

proc key_matrix {keynum state} {
	set key_pressed $keynum
	set key [string trim "[osd info kb.$keynum.text -text]"]

	#how dirty is this?
	set km keymatrix$state

	#info from http://map.grauw.nl/articles/keymatrix.php (thanks Grauw)

	switch $key {
		"0" 	{$km 0 1}
		"1" 	{$km 0 2}
		"2" 	{$km 0 4}
		"3" 	{$km 0 8}
		"4" 	{$km 0 16}
		"5" 	{$km 0 32}
		"6" 	{$km 0 64}
		"7" 	{$km 0 128}

		"8" 	{$km 1 1}
		"9" 	{$km 1 2}
		"-" 	{$km 1 4}
		"=" 	{$km 1 8}
		"\\" 	{$km 1 16}
		"\[" 	{$km 1 32}
		"\]" 	{$km 1 64}
		";" 	{$km 1 128}

		"'" 	{$km 2 1}
		"`" 	{$km 2 2}
		"," 	{$km 2 4}
		"." 	{$km 2 8}
		"/" 	{$km 2 16}
		"dead" 	{$km 2 32}
		"A" 	{$km 2 64}
		"B" 	{$km 2 128}

		"C" 	{$km 3 1}
		"D" 	{$km 3 2}
		"E" 	{$km 3 4}
		"F" 	{$km 3 8}
		"G" 	{$km 3 16}
		"H" 	{$km 3 32}
		"I" 	{$km 3 64}
		"J" 	{$km 3 128}

		"K" 	{$km 4 1}
		"L" 	{$km 4 2}
		"M" 	{$km 4 4}
		"N" 	{$km 4 8}
		"O" 	{$km 4 16}
		"P" 	{$km 4 32}
		"Q" 	{$km 4 64}
		"R" 	{$km 4 128}

		"S" 	{$km 5 1}
		"T" 	{$km 5 2}
		"U" 	{$km 5 4}
		"V" 	{$km 5 8}
		"W" 	{$km 5 16}
		"X" 	{$km 5 32}
		"Y" 	{$km 5 64}
		"Z" 	{$km 5 128}

		"shift" {$km 6 1}
		"ctrl" 	{$km 6 2}
		"graph" {$km 6 4}
		"caps" 	{$km 6 8}
		"code" 	{$km 6 16}
		"f-1" 	{$km 6 32}
		"f-2" 	{$km 6 64}
		"f-3" 	{$km 6 128}

		"f-4" 	{$km 7 1}
		"f-5" 	{$km 7 2}
		"esc" 	{$km 7 4}
		"tab" 	{$km 7 8}
		"stop" 	{$km 7 16}
		"bs" 	{$km 7 32}
		"select" {$km 7 64}
		"return" {$km 7 128}
		"<--"	{$km 7 128}

		"space" {$km 8 1}
		"home" 	{$km 8 2}
		"ins" 	{$km 8 4}
		"del" 	{$km 8 8}
	}

		#cursor keys etc (not implemented... should we?)
		#numeric keyboard?
}

namespace export toggle_osd_keyboard

};# namespace osd_keyboard

namespace import osd_keyboard::*
