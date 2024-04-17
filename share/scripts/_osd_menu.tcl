namespace eval osd_menu {

variable is_dingux [string match dingux "[openmsx_info platform]"]

variable menu_z 3

proc get_optional {dict_name key default} {
	upvar $dict_name d
	expr {[dict exists $d $key] ? [dict get $d $key] : $default}
}

proc set_optional {dict_name key value} {
	upvar $dict_name d
	if {![dict exists $d $key]} {
		dict set d $key $value
	}
}

variable menulevels 0
# some variables for typing-in-menus support
variable input_buffer ""
variable input_last_time [openmsx_info realtime]
variable input_timeout 1; #sec

variable controller_capture_timeout 5; #sec
variable controller_capture_after_id ""

variable pool_prefix "pool::"

proc push_menu_info {} {
	variable menulevels
	incr menulevels 1
	set levelname "menuinfo_$menulevels"
	variable $levelname
	set $levelname [uplevel {dict create \
		name $name lst $lst menu_len $menu_len presentation $presentation \
		menutextexpressions $menutextexpressions menutexts $menutexts \
		selectinfo $selectinfo selectidx $selectidx scrollidx \
		$scrollidx on_close $on_close osditems $osditems}]
}

proc peek_menu_info {} {
	variable menulevels
	uplevel upvar #0 osd_menu::menuinfo_$menulevels menuinfo
}

proc set_selectidx {value} {
	peek_menu_info
	dict set menuinfo selectidx $value
}

proc set_scrollidx {value} {
	peek_menu_info
	dict set menuinfo scrollidx $value
}

proc get_current_menu_name {} {
	peek_menu_info
	return [dict get $menuinfo name]
}

proc menu_create {menudef} {
	variable menulevels
	variable default_bg_color
	variable default_text_color
	variable default_select_color
	variable default_header_text_color
	variable menu_z

	set name "menu[expr {$menulevels + 1}]"

	set defactions   [get_optional menudef "actions" ""]
	set bgcolor      [get_optional menudef "bg-color" $default_bg_color]
	set deftextcolor [get_optional menudef "text-color" $default_text_color]
	set selectcolor  [get_optional menudef "select-color" $default_select_color]
	set deffontsize  [get_optional menudef "font-size" 12]
	set deffont      [get_optional menudef "font" "skins/DejaVuSans.ttf.gz"]
	# set default border to 0.5 to avoid content ending up in the outer
	# (black) edge
	set bordersize   [get_optional menudef "border-size" 0.5]
	set on_open      [get_optional menudef "on-open" ""]
	set on_close     [get_optional menudef "on-close" ""]

	osd create rectangle $name -scaled true -rgba $bgcolor -clip true \
		-borderrgba 0x000000ff -bordersize 0.5 -z $menu_z

	set bordersizereduction [expr {-$bordersize*2}]

	# we manage x border for texts by clip rect, y border by position of
	# items
	osd create rectangle $name.clip -x $bordersize -w $bordersizereduction \
		-relw 1 -relh 1 -scaled true -alpha 0 -clip true

	# note that y can non-integer, so we can't use incr on it
	set y $bordersize
	set selectinfo [list]
	set menutextexpressions [list]
	set menutexts [list]
	set osditems [list]
	set itemidx 0
	foreach itemdef [dict get $menudef items] {
		set selectable  [get_optional itemdef "selectable" true]
		set y         [expr {$y + [get_optional itemdef "pre-spacing" 0]}]
		set fontsize    [get_optional itemdef "font-size" $deffontsize]
		set font        [get_optional itemdef "font"      $deffont]
		set textcolor [expr {$selectable
		              ? [get_optional itemdef "text-color" $deftextcolor]
		              : [get_optional itemdef "text-color" $default_header_text_color]}]
		set actions     [get_optional itemdef "actions"     ""]
		set on_select   [get_optional itemdef "on-select"   ""]
		set on_deselect [get_optional itemdef "on-deselect" ""]
		set textid "${name}.clip.item${itemidx}"
		if {[dict exists $itemdef textexpr]} {
			set textexpr [dict get $itemdef textexpr]
			lappend menutextexpressions $textid $textexpr
		} else {
			set text [dict get $itemdef text]
			lappend menutexts $textid $text
		}
		osd create text $textid -font $font -size $fontsize \
					-rgba $textcolor -x 0 -y $y

		lappend osditems $textid

		if {$selectable} {
			set allactions [concat $defactions $actions]
			# NOTE: the 1.5 below is necessary to place the select
			# bar properly vertically... It's unclear why this is
			# necessary and why this emperically determined value
			# is working.
			lappend selectinfo [list [expr {$y+1.5}] $fontsize $allactions $on_select $on_deselect $textid]
		}
		set y [expr {$y + $fontsize}]
		set y [expr {$y + [get_optional itemdef "post-spacing" 0]}]

		incr itemidx
	}

	set width [dict get $menudef width]
	set height [expr {$y + $bordersize}]
	set xpos [get_optional menudef "xpos" [expr {(320 - $width)  / 2}]]
	set ypos [get_optional menudef "ypos" [expr {(240 - $height) / 2}]]
	osd configure $name -x $xpos -y $ypos -w $width -h $height

	osd create rectangle "${name}.selection" -z -1 -rgba $selectcolor \
		-x 0 -w $width

	set lst [get_optional menudef "lst" ""]
	set menu_len [get_optional menudef "menu_len" 0]
	if {[llength $lst] > $menu_len} {
		set startheight 0
		if {[llength $selectinfo] > 0} {
			# there are selectable items. Start the scrollbar
			# at the top of the first selectable item
			# (skipping headers and stuff)
			set startheight [lindex $selectinfo 0 0]
		}
		set scrollbarwidth 6
		osd create rectangle "${name}.scrollbar" -rgba 0x00000010 \
		   -relx 1.0 -x -$scrollbarwidth -w $scrollbarwidth -relh 1.0 -h -$startheight -y $startheight -borderrgba 0x00000070 -bordersize 0.5
		osd create rectangle "${name}.scrollbar.thumb" -rgba $default_select_color \
		   -relw 1.0 -w -2 -x 1
		# Now also reduce the width of the clip area to avoid having
		# text on the scrollbar...
		# For now let's just keep only a fixed 0.5 margin between text
		# and scrollbar at the right size:
		osd configure $name.clip -w [expr {-$bordersize - $scrollbarwidth - 0.5}]
		# Alternatively, keep the bordersize intact at the right side
		# next to the scrollbar:
		# osd configure $name.clip -w [expr {$bordersizereduction - $scrollbarwidth}]
		# Also the select bar shouldn't be on the scrollbar:
		osd configure "${name}.selection" -w [expr {$width - $scrollbarwidth}]
	}
	set presentation [get_optional menudef "presentation" ""]
	set selectidx 0
	set scrollidx 0
	push_menu_info

	uplevel #0 $on_open
	menu_on_select $selectinfo $selectidx

	menu_refresh_top
	menu_update_scrollbar
}

proc menu_update_scrollbar {} {
	peek_menu_info
	set name [dict get $menuinfo name]
	if {[osd exists ${name}.scrollbar]} {
		set menu_len   [dict get $menuinfo menu_len]
		set scrollidx  [dict get $menuinfo scrollidx]
		set selectidx  [dict get $menuinfo selectidx]
		set totalitems [llength [dict get $menuinfo lst]]
		set height [expr {1.0*$menu_len/$totalitems}]
		set minheight 0.05 ;# TODO: derive from width of bar
		set height [expr {$height > $minheight ? $height : $minheight}]
		set pos [expr {1.0*($scrollidx+$selectidx)/($totalitems-1)}]
		# scale the pos to the usable range
		set pos [expr {$pos*(1.0-$height)}]
		osd configure "${name}.scrollbar.thumb" -relh $height -rely $pos
	}
}

proc menu_refresh_top {} {
	peek_menu_info
	foreach {osdid textexpr} [dict get $menuinfo menutextexpressions] {
		set cmd [list subst $textexpr]
		osd configure $osdid -text [uplevel #0 $cmd]
	}
	foreach {osdid text} [dict get $menuinfo menutexts] {
		osd configure $osdid -text $text
	}

	set selectinfo [dict get $menuinfo selectinfo]
	if {[llength $selectinfo] == 0} return
	set selectidx  [dict get $menuinfo selectidx ]
	lassign [lindex $selectinfo $selectidx] sely selh
	osd configure "[dict get $menuinfo name].selection" -y $sely -h $selh
}

proc menu_close_top {} {
	variable menulevels
	peek_menu_info
	menu_on_deselect [dict get $menuinfo selectinfo] [dict get $menuinfo selectidx]
	uplevel #0 [dict get $menuinfo on_close]
	osd destroy [dict get $menuinfo name]
	unset menuinfo
	incr menulevels -1
	if {$menulevels == 0} {
		menu_last_closed
	}
}

proc menu_close_all {} {
	variable menulevels
	while {$menulevels} {
		menu_close_top
	}
}

proc menu_close_until {name} {
	variable menulevels
	while {$menulevels && [get_current_menu_name] != $name} {
		menu_close_top
	}
}

proc menu_setting {cmd_result} {
	menu_refresh_top
}

proc menu_updown {delta} {
	peek_menu_info
	set num [llength [dict get $menuinfo selectinfo]]
	if {$num == 0} return
	set old_idx [dict get $menuinfo selectidx]
	menu_reselect [expr {($old_idx + $delta) % $num}]
}

proc menu_reselect {new_idx} {
	peek_menu_info
	set selectinfo [dict get $menuinfo selectinfo]
	set old_idx [dict get $menuinfo selectidx]
	menu_on_deselect $selectinfo $old_idx
	set_selectidx $new_idx
	menu_on_select $selectinfo $new_idx
	menu_refresh_top
}

proc menu_on_select {selectinfo selectidx} {
	set osd_elem [lindex $selectinfo $selectidx 5]
	if {$osd_elem ne ""} {
		osd configure $osd_elem -scrollSpeed 20 -scrollPauseLeft 1.0 -scrollPauseRight 0.4
	}
	set on_select [lindex $selectinfo $selectidx 3]
	uplevel #0 $on_select
}

proc menu_on_deselect {selectinfo selectidx} {
	set osd_elem [lindex $selectinfo $selectidx 5]
	if {$osd_elem ne ""} {
		osd configure $osd_elem -scrollSpeed 0
	}
	set on_deselect [lindex $selectinfo $selectidx 4]
	uplevel #0 $on_deselect
}

proc menu_action {button} {
	# for any menu action, clear the input buffer
	variable input_buffer
	set input_buffer ""

	peek_menu_info
	set selectidx [dict get $menuinfo selectidx ]
	menu_action_idx $selectidx $button
}
proc menu_action_idx {idx button} {
	peek_menu_info
	set selectinfo [dict get $menuinfo selectinfo]
	set actions [lindex $selectinfo $idx 2]
	set_optional actions UP   {osd_menu::menu_updown -1}
	set_optional actions DOWN {osd_menu::menu_updown  1}
	set_optional actions B    {osd_menu::menu_close_top}
	set cmd [get_optional actions $button ""]
	uplevel #0 $cmd
}

proc get_mouse_coords {} {
	peek_menu_info
	set x 2; set y 2
	catch {
		set name [dict get $menuinfo name]
		lassign [osd info $name -mousecoord] x y
	}
	list $x $y
}
proc menu_get_mouse_idx {xy} {
	lassign $xy x y
	if {$x < 0 || 1 < $x || $y < 0 || 1 < $y} {return -1}

	peek_menu_info
	set name [dict get $menuinfo name]

	set yy [expr {$y * [osd info $name -h]}]
	set sel 0
	foreach i [dict get $menuinfo selectinfo] {
		lassign $i y h actions
		if {($y <= $yy) && ($yy < ($y + $h))} {
			return $sel
		}
		incr sel
	}
	return -1
}
proc menu_mouse_down {} {
	variable mouse_coord
	variable mouse_idx
	set mouse_coord [get_mouse_coords]
	set mouse_idx [menu_get_mouse_idx $mouse_coord]
	if {$mouse_idx != -1} {
		menu_reselect $mouse_idx
	}
}
proc menu_mouse_up {} {
	variable mouse_coord
	variable mouse_idx
	set mouse_coord [get_mouse_coords]
	set mouse_idx [menu_get_mouse_idx $mouse_coord]
	if {$mouse_idx != -1} {
		menu_action_idx $mouse_idx A
	}
	unset mouse_coord
	unset mouse_idx
}
proc menu_mouse_wheel {event} {
	lassign $event type1 type2 x y
	if {$y > 0} {
		osd_menu::menu_action UP
	} elseif {$y < 0} {
		osd_menu::menu_action DOWN
	}
}
proc menu_mouse_motion {} {
	variable mouse_coord
	variable mouse_idx
	if {![info exists mouse_coord]} return

	set new_mouse_coord [get_mouse_coords]
	set new_idx [menu_get_mouse_idx $new_mouse_coord]
	if {$new_idx != -1 && $new_idx != $mouse_idx} {
		menu_reselect $new_idx
		set mouse_coord $new_mouse_coord
		set mouse_idx $new_idx
		return
	}

	if {$mouse_idx != -1} {
		lassign $mouse_coord     old_x old_y
		lassign $new_mouse_coord new_x new_y
		set delta_x [expr {$new_x - $old_x}]
		set delta_y [expr {$new_y - $old_y}]
		if {$delta_y > 0.1} {
			menu_action_idx $mouse_idx DOWN
			set mouse_coord $new_mouse_coord
			return
		} elseif {$delta_y < -0.1} {
			menu_action_idx $mouse_idx UP
			set mouse_coord $new_mouse_coord
			return
		} elseif {$delta_x > 0.1} {
			menu_action_idx $mouse_idx RIGHT
			set mouse_coord $new_mouse_coord
			return
		} elseif {$delta_x < -0.1} {
			menu_action_idx $mouse_idx LEFT
			set mouse_coord $new_mouse_coord
			return
		}
	}
}

proc do_menu_open {top_menu} {
	variable is_dingux

	# close console, because the menu interferes with it
	set ::console off

	# also remove other OSD controlled widgets (like the osd keyboard)
	if {[info exists ::osd_control::close]} {
		eval $::osd_control::close
	}
	# end tell how to close this widget
	namespace eval ::osd_control {set close ::osd_menu::main_menu_close}

	menu_create $top_menu

	set ::pause true
	# TODO make these bindings easier to customize
	bind -layer osd_menu "OSDcontrol UP PRESS"    -repeat {osd_menu::menu_action UP   }
	bind -layer osd_menu "OSDcontrol DOWN PRESS"  -repeat {osd_menu::menu_action DOWN }
	bind -layer osd_menu "OSDcontrol LEFT PRESS"  -repeat {osd_menu::menu_action LEFT }
	bind -layer osd_menu "OSDcontrol RIGHT PRESS" -repeat {osd_menu::menu_action RIGHT}
	bind -layer osd_menu "mouse button1 down" {osd_menu::menu_mouse_down}
	bind -layer osd_menu "mouse button1 up"   {osd_menu::menu_mouse_up}
	bind -layer osd_menu "mouse button3 up"   {osd_menu::menu_close_top}
	bind -layer osd_menu "mouse motion"       {osd_menu::menu_mouse_motion}
	bind -layer osd_menu "mouse wheel" -event {osd_menu::menu_mouse_wheel}
	if {$is_dingux} {
		bind -layer osd_menu "keyb LCTRL"  {osd_menu::menu_action A    }
		bind -layer osd_menu "keyb LALT"   {osd_menu::menu_action B    }
	} else {
		bind -layer osd_menu "OSDcontrol A PRESS" {osd_menu::menu_action A }
		bind -layer osd_menu "OSDcontrol B PRESS" {osd_menu::menu_action B }
	}
	bind -layer osd_menu "CTRL+UP"      {osd_menu::select_menu_idx 0}
	bind -layer osd_menu "CTRL+LEFT"    {osd_menu::select_menu_idx 0}
	bind -layer osd_menu "keyb HOME"    {osd_menu::select_menu_idx 0}
	set alphanum {a b c d e f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 MINUS SHIFT+MINUS}
	foreach char $alphanum {
		bind -layer osd_menu "keyb $char"      "osd_menu::handle_keyboard_input $char"
	}
	activate_input_layer osd_menu -blocking
}

proc menu_last_closed {} {
	set ::pause false
	after realtime 0 {deactivate_input_layer osd_menu}

	namespace eval ::osd_control {unset close}
}

proc prepare_menu_list {lst num menudef} {
	set execute [dict get $menudef execute]
	set header  [dict get $menudef header]
	set item_extra   [get_optional menudef item ""]
	set on_select    [get_optional menudef on-select ""]
	set on_deselect  [get_optional menudef on-deselect ""]
	set presentation [get_optional menudef presentation $lst]
	# 'assert': presentation should have same length as item list!
	if {[llength $presentation] != [llength $lst]} {
		error "Presentation should be of same length as item list!"
	}
	dict set menudef presentation $presentation
	lappend header "selectable" "false"
	set items [list $header]
	set lst_len [llength $lst]
	set menu_len [expr {$lst_len < $num ? $lst_len : $num}]
	for {set i 0} {$i < $menu_len} {incr i} {
		set actions [list "A" "osd_menu::list_menu_item_exec {$execute} $i"]
		if {$i == 0} {
			lappend actions "UP" "osd_menu::move_selection -1"
		}
		if {$i == ($menu_len - 1)} {
			lappend actions "DOWN" "osd_menu::move_selection 1"
		}
		lappend actions "LEFT"  "osd_menu::move_selection -$menu_len"
		lappend actions "RIGHT" "osd_menu::move_selection  $menu_len"
		set item [list "textexpr" "\[osd_menu::list_menu_item_show $i\]" \
		               "actions" $actions]
		if {$on_select ne ""} {
			lappend item "on-select" "osd_menu::list_menu_item_select $i $on_select"
		}
		if {$on_deselect ne ""} {
			lappend item "on-deselect" "osd_menu::list_menu_item_select $i $on_deselect"
		}
		lappend items [concat $item $item_extra]
	}
	dict set menudef items $items
	dict set menudef lst $lst
	dict set menudef menu_len $menu_len
	return $menudef
}

proc list_menu_item_exec {execute pos} {
	peek_menu_info
	{*}$execute [lindex [dict get $menuinfo lst] [expr {$pos + [dict get $menuinfo scrollidx]}]]
}

proc list_menu_item_show {pos} {
	peek_menu_info
	return [lindex [dict get $menuinfo presentation] [expr {$pos + [dict get $menuinfo scrollidx]}]]
}

proc list_menu_item_select {pos select_proc} {
	peek_menu_info
	$select_proc [lindex [dict get $menuinfo lst] [expr {$pos + [dict get $menuinfo scrollidx]}]]
}

proc move_selection {delta} {
	peek_menu_info
	set lst_last [expr {[llength [dict get $menuinfo lst]] - 1}]
	set scrollidx [dict get $menuinfo scrollidx]
	set selectidx [dict get $menuinfo selectidx]

	set old_itemidx [expr {$scrollidx + $selectidx}]
	set new_itemidx [expr {$old_itemidx + $delta}]

	if {$new_itemidx < 0} {
		# Before first element
		if {$old_itemidx == 0} {
			# if first element was already selected, wrap to last
			set new_itemidx $lst_last
		} else {
			# otherwise, clamp to first element
			set new_itemidx 0
		}
	} elseif {$new_itemidx > $lst_last} {
		# After last element
		if {$old_itemidx == $lst_last} {
			# if last element was already selected, wrap to first
			set new_itemidx 0
		} else {
			# otherwise clam to last element
			set new_itemidx $lst_last
		}
	}

	select_menu_idx $new_itemidx
}

proc select_menu_idx {itemidx} {
	peek_menu_info
	set menu_len   [dict get $menuinfo menu_len]
	set scrollidx  [dict get $menuinfo scrollidx]
	set selectidx  [dict get $menuinfo selectidx]
	set selectinfo [dict get $menuinfo selectinfo]

	menu_on_deselect $selectinfo $selectidx

	set selectidx [expr {$itemidx - $scrollidx}]
	if {$selectidx < 0} {
		incr scrollidx $selectidx
		set selectidx 0
	} elseif {($menu_len > 0) && ($selectidx >= $menu_len)} {
		set selectidx [expr {$menu_len - 1}]
		set scrollidx [expr {$itemidx - $selectidx}]
	}

	set_selectidx $selectidx
	set_scrollidx $scrollidx
	menu_on_select $selectinfo $selectidx
	menu_refresh_top
	menu_update_scrollbar
}

proc select_menu_item {item} {
	peek_menu_info

	set index [lsearch -exact [dict get $menuinfo lst] $item]
	if {$index == -1} return

	select_menu_idx $index
}

proc handle_keyboard_input {char} {
	variable input_buffer
	variable input_last_time
	variable input_timeout

	if {$char eq "MINUS"} {
		set char "-"
	} elseif {$char eq "SHIFT+MINUS"} {
		set char "_"
	}

	set current_time [openmsx_info realtime]
	if {[expr {$current_time - $input_last_time}] < $input_timeout} {
		set input_buffer "$input_buffer$char"
	} else {
		set input_buffer $char
	}
	set input_last_time $current_time

	osd_menu::select_next_menu_item_starting_with $input_buffer
}

proc select_next_menu_item_starting_with {text} {
	peek_menu_info

	set items [dict get $menuinfo presentation]
	if {[llength $items] == 0} return

	set selectidx [dict get $menuinfo selectidx]
	set scrollidx [dict get $menuinfo scrollidx]
	set itemidx [expr {$scrollidx + $selectidx}]

	# start after the current item if this is a new search
	if {[string length $text] == 1} {
		incr itemidx
	}
	# use the list twice to wrap
	set index [lsearch -glob -nocase -start $itemidx [concat $items $items] "$text*"]
	if {$index == -1} return
	set index [expr {$index % [llength $items]}]

	select_menu_idx $index
}

} ;# namespace osd_menu
