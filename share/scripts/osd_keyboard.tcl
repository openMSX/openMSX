namespace eval osd_keyboard {

#KNOWN ISSUES
# Any other rendered than SDL doesn't display the keyboard background correctly
#
# todo: a lot


#init vars
variable mouse1_pressed false
variable keycount 0
variable key_pressed 0

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
		for {set i 0;} {$i <= 8} {incr i;} {
			keymatrixup $i 255
		}
		return ""
	}

	variable mouse1_pressed false
	variable keycount 0

	#bind stuff
	bind_default "mouse button1 down" 	{osd_keyboard::key_handeler true}
	bind_default "mouse button1 up" 	{osd_keyboard::key_handeler false}

	bind_default "mouse button3 down" 	{osd_keyboard::key_hold}

	#Define Keyboard (how do we handle the shift/ctrl/graph command?)
	set rows {"f-1*16|f-2*16|f-3*16|f-4*16|f-5*16|null*24|select|stop|home|ins|del" \
			 "esc|1|2|3|4|5|6|7|8|9|0|-|=|\\|bs" \
			 "tab*13|Q|W|E|R|T|Y|U|I|O|P|\[|]" \
			 "ctrl*15|A|S|D|F|G|H|J|K|L|;|'|`|ret" \
			 "shift*21|Z|X|C|V|B|N|M|,|.|/|dead|shift*21" \
			 "null*21|caps|graph|space*87|code"}

	#Draw Keyboard
	osd_widgets::msx_init kb

	osd create rectangle kb.bg -h 59 -w 164 -rgba 0x88888880

	for {set y 0;} {$y <= [llength $rows]} {incr y;} {
		set x 0
		set total 0
		foreach {keys} [split [lindex $rows $y]  "|"] {
			set key [split $keys "*"]
			set key_text [lindex $key 0]
			set key_width [lindex $key 1]

			set keycolor 0xffffffc0

			if {$key_width < 1} {set key_width 10;}


			if {$key_text != "null"} {
				osd create rectangle kb.$keycount	-relx $total \
													-rely [expr $y*10] \
													-relh 9 \
													-relw $key_width \
													-rgba $keycolor \

				osd create text kb.$keycount.text 	-x 0.1 \
													-y 0.1 \
													-text $key_text \
													-size 4

				#enter key special handeling

				if {$key_text == "ret"} {
					osd configure kb.$keycount \
						-x [expr $x - 13] -y [expr $y - 13] -w 6 -h 10
				}

				incr x
				incr keycount
			}

			set total [expr $total + $key_width + 1]
		}
	}
	#sorry dirty solution
	incr keycount -1

	return ""
}

proc key_hold {} {
	variable keycount
	for {set i 0;} {$i <= $keycount} {incr i;} {
			foreach {x y} [osd info "kb.$i" -mousecoord] {}
			if {($x >= 0 && $x <= 1) && ($y >= 0 && $y <= 1)} {
				key_matrix $i down;osd configure kb.$i -rgba 0x00ff88c0
			}
	}
}

proc key_handeler {mouse_state} {
	variable keycount
	variable key_pressed
	#scan which key is down (can be optimized but for now it's ok)
	if {$mouse_state} {
		for {set i 0;} {$i <= $keycount} {incr i;} {
		foreach {x y} [osd info "kb.$i" -mousecoord] {}
		if {($x >= 0 && $x <= 1) && ($y >= 0 && $y <= 1)} {
			osd configure kb.$i -rgba 0xffcc8880
			key_matrix $i down
			set key_pressed $i
			}
		}
	} else {osd configure kb.$key_pressed -rgba 0xffffffc0;key_matrix $key_pressed up}
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
		"ret" 	{$km 7 128}

		"space" {$km 8 1}
		"home" 	{$km 8 2}
		"ins" 	{$km 8 4}
		"del" 	{$km 8 8}
	}

		#cursor keys etc (not implemented... should we?)
		#numeric keyboard?
}

namespace export toggle_osd_keyboard
}

namespace import osd_keyboard::*
