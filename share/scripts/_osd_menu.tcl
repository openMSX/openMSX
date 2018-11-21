namespace eval osd_menu {

set_help_text main_menu_open   "Show the OSD menu."
set_help_text main_menu_close  "Remove the OSD menu."
set_help_text main_menu_toggle "Toggle the OSD menu."

variable is_dingux [string match dingux "[openmsx_info platform]"]
variable scaling_available [expr {[lindex [lindex [openmsx_info setting scale_factor] 2] 1] > 1}]

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

proc push_menu_info {} {
	variable menulevels
	incr menulevels 1
	set levelname "menuinfo_$menulevels"
	variable $levelname
	set $levelname [uplevel {dict create \
		name $name lst $lst menu_len $menu_len presentation $presentation \
		menutexts $menutexts selectinfo $selectinfo selectidx $selectidx \
		scrollidx $scrollidx on_close $on_close}]
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

proc menu_create {menudef} {
	variable menulevels
	variable default_bg_color
	variable default_text_color
	variable default_select_color
	variable default_header_text_color

	set name "menu[expr {$menulevels + 1}]"

	set defactions   [get_optional menudef "actions" ""]
	set bgcolor      [get_optional menudef "bg-color" $default_bg_color]
	set deftextcolor [get_optional menudef "text-color" $default_text_color]
	set selectcolor  [get_optional menudef "select-color" $default_select_color]
	set deffontsize  [get_optional menudef "font-size" 12]
	set deffont      [get_optional menudef "font" "skins/Vera.ttf.gz"]
	set bordersize   [get_optional menudef "border-size" 0]
	set on_open      [get_optional menudef "on-open" ""]
	set on_close     [get_optional menudef "on-close" ""]

	osd create rectangle $name -scaled true -rgba $bgcolor -clip true \
		-borderrgba 0x000000ff -bordersize 0.5

	set y $bordersize
	set selectinfo [list]
	set menutexts [list]
	foreach itemdef [dict get $menudef items] {
		set selectable  [get_optional itemdef "selectable" true]
		incr y          [get_optional itemdef "pre-spacing" 0]
		set fontsize    [get_optional itemdef "font-size" $deffontsize]
		set font        [get_optional itemdef "font"      $deffont]
		set textcolor [expr {$selectable
		              ? [get_optional itemdef "text-color" $deftextcolor]
		              : [get_optional itemdef "text-color" $default_header_text_color]}]
		set actions     [get_optional itemdef "actions"     ""]
		set on_select   [get_optional itemdef "on-select"   ""]
		set on_deselect [get_optional itemdef "on-deselect" ""]
		set textid "${name}.item${y}"
		set text [dict get $itemdef text]
		lappend menutexts $textid $text
		osd create text $textid -font $font -size $fontsize \
					-rgba $textcolor -x $bordersize -y $y
		if {$selectable} {
			set allactions [concat $defactions $actions]
			lappend selectinfo [list $y $fontsize $allactions $on_select $on_deselect]
		}
		incr y $fontsize
		incr y [get_optional itemdef "post-spacing" 0]
	}

	set width [dict get $menudef width]
	set height [expr {$y + $bordersize}]
	set xpos [get_optional menudef "xpos" [expr {(320 - $width)  / 2}]]
	set ypos [get_optional menudef "ypos" [expr {(240 - $height) / 2}]]
	osd configure $name -x $xpos -y $ypos -w $width -h $height

	set selw [expr {$width - 2 * $bordersize}]
	osd create rectangle "${name}.selection" -z -1 -rgba $selectcolor \
		-x $bordersize -w $selw

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
		osd create rectangle "${name}.scrollbar" -z -1 -rgba 0x00000010 \
		   -relx 1.0 -x -6 -w 6 -relh 1.0 -h -$startheight -y $startheight -borderrgba 0x00000070 -bordersize 0.5
		osd create rectangle "${name}.scrollbar.thumb" -z -1 -rgba $default_select_color \
		   -relw 1.0 -w -2 -x 1
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
	foreach {osdid text} [dict get $menuinfo menutexts] {
		set cmd [list subst $text]
		osd configure $osdid -text [uplevel #0 $cmd]
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
	set on_select [lindex $selectinfo $selectidx 3]
	uplevel #0 $on_select
}

proc menu_on_deselect {selectinfo selectidx} {
	set on_deselect [lindex $selectinfo $selectidx 4]
	uplevel #0 $on_deselect
}

proc menu_action {button} {
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
	set name [dict get $menuinfo name]
	set x 2; set y 2
	catch {lassign [osd info $name -mousecoord] x y}
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

user_setting create string osd_rom_path "OSD Rom Load Menu Last Known Path" $env(HOME)
user_setting create string osd_disk_path "OSD Disk Load Menu Last Known Path" $env(HOME)
user_setting create string osd_tape_path "OSD Tape Load Menu Last Known Path" $env(HOME)
user_setting create string osd_hdd_path "OSD HDD Load Menu Last Known Path" $env(HOME)
user_setting create string osd_ld_path "OSD LD Load Menu Last Known Path" $env(HOME)

if {![file exists $::osd_rom_path] || ![file readable $::osd_rom_path]} {
	# revert to default (should always exist)
	unset ::osd_rom_path
}

if {![file exists $::osd_disk_path] || ![file readable $::osd_disk_path]} {
	# revert to default (should always exist)
	unset ::osd_disk_path
}

if {![file exists $::osd_tape_path] || ![file readable $::osd_tape_path]} {
	# revert to default (should always exist)
	unset ::osd_tape_path
}

if {![file exists $::osd_hdd_path] || ![file readable $::osd_hdd_path]} {
	# revert to default (should always exist)
	unset ::osd_hdd_path
}

if {![file exists $::osd_ld_path] || ![file readable $::osd_ld_path]} {
	# revert to default (should always exist)
	unset ::osd_ld_path
}


variable taperecordings_directory [file normalize $::env(OPENMSX_USER_DATA)/../taperecordings]

proc main_menu_open {} {
	do_menu_open [create_main_menu]
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
	bind -layer osd_menu "mouse button4 down" {osd_menu::menu_action UP   }
	bind -layer osd_menu "mouse button5 down" {osd_menu::menu_action DOWN }
	if {$is_dingux} {
		bind -layer osd_menu "keyb LCTRL"  {osd_menu::menu_action A    }
		bind -layer osd_menu "keyb LALT"   {osd_menu::menu_action B    }
	} else {
		bind -layer osd_menu "OSDcontrol A PRESS" {osd_menu::menu_action A }
		bind -layer osd_menu "OSDcontrol B PRESS" {osd_menu::menu_action B }
		# on Android, use BACK button to go back in menus
		bind -layer osd_menu "keyb WORLD_92"      {osd_menu::menu_action B }
	}
	activate_input_layer osd_menu -blocking
}

proc main_menu_close {} {
	menu_close_all
}

proc main_menu_toggle {} {
	variable menulevels
	if {$menulevels} {
		# there is at least one menu open, close it
		menu_close_all
	} else {
		# none open yet, open main menu
		main_menu_open
	}
}

proc menu_last_closed {} {
	variable is_dingux

	set ::pause false
	deactivate_input_layer osd_menu

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
		set item [list "text" "\[osd_menu::list_menu_item_show $i\]" \
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
	} elseif {$selectidx >= $menu_len} {
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

#
# definitions of menus
#
proc create_main_menu {} {
	set menu_def {
		font-size 10
		border-size 2
		width 160
	}
	lappend items { text "[openmsx_info version]"
	         font-size 12
	         post-spacing 6
	         selectable false }
	if {[catch carta]} {; # example: Philips NMS 801
		lappend items { text "(No cartridge slot available...)"
			selectable false
			text-color 0x808080ff
		}
	} else {
		foreach slot [lrange [lsort [info command cart?]] 0 1] {
			set slot_str [string toupper [string index $slot end]]
			lappend items [list text "Load ROM... (slot $slot_str)" \
				actions [list A "osd_menu::menu_create \[osd_menu::menu_create_ROM_list \$::osd_rom_path $slot\]"]]
		}
	}
	if {[catch diska]} {
		lappend items { text "(No disk drives available...)"
			selectable false
			text-color 0x808080ff
		}
	} else {
		foreach drive [lrange [lsort [info command disk?]] 0 1] {
			set drive_str [string toupper [string index $drive end]]
			lappend items [list text "Insert Disk... (drive $drive_str)" \
				actions [list A "osd_menu::menu_create \[osd_menu::menu_create_disk_list \$::osd_disk_path $drive\]"]]
		}
	}
	if {[info command hda] ne ""} {; # only exists when hard disk extension available
		foreach drive [lrange [lsort [info command hd?]] 0 1] {
			set drive_str [string toupper [string index $drive end]]
			lappend items [list text "Change HD/SD image... (drive $drive_str)" \
				actions [list A "osd_menu::menu_create \[osd_menu::menu_create_hdd_list \$::osd_hdd_path $drive\]"]]
		}
	}
	if {[info command laserdiscplayer] ne ""} {; # only exists on some Pioneers
		lappend items { text "Load LaserDisc..."
			actions { A { osd_menu::menu_create [osd_menu::menu_create_ld_list $::osd_ld_path]} }
		}
	}
	if {[catch "machine_info connector cassetteport"]} {; # example: turboR
		lappend items { text "(No cassette port present...)"
			selectable false
			text-color 0x808080ff
			post-spacing 3
		}
	} else {
		lappend items { text "Set Tape..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_tape_list $::osd_tape_path]} }
	         post-spacing 3 }
	}
	lappend items { text "Save State..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_save_state] }}}
	lappend items { text "Load State..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_load_state] }}
	         post-spacing 3 }
	lappend items { text "Hardware..."
	         actions { A { osd_menu::menu_create $osd_menu::hardware_menu }}
	         post-spacing 3 }
	lappend items { text "Misc Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::misc_setting_menu }}}
	lappend items { text "Sound Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::sound_setting_menu }}}
	lappend items { text "Video Settings..."
	         actions { A { osd_menu::menu_create [osd_menu::create_video_setting_menu] }}
	         post-spacing 3 }
	lappend items { text "Advanced..."
	         actions { A { osd_menu::menu_create $osd_menu::advanced_menu }}
	         post-spacing 10 }
	lappend items { text "Reset MSX"
	         actions { A { reset; osd_menu::menu_close_all }}}
	lappend items { text "Exit openMSX"
	         actions { A quitmenu::quit_menu }}
	dict set menu_def items $items
	return $menu_def
}

set misc_setting_menu {
	font-size 8
	border-size 2
	width 150
	xpos 100
	ypos 120
	items {{ text "Misc Settings"
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Speed: $speed"
	         actions { LEFT  { osd_menu::menu_setting [incr speed -1] }
	                   RIGHT { osd_menu::menu_setting [incr speed  1] }}}
	       { text "Minimal Frameskip: $minframeskip"
	         actions { LEFT  { osd_menu::menu_setting [incr minframeskip -1] }
	                   RIGHT { osd_menu::menu_setting [incr minframeskip  1] }}}
	       { text "Maximal Frameskip: $maxframeskip"
	         actions { LEFT  { osd_menu::menu_setting [incr maxframeskip -1] }
	                   RIGHT { osd_menu::menu_setting [incr maxframeskip  1] }}}}}

set resampler_desc [dict create fast "fast (but low quality)" blip "blip (good speed/quality)" hq "hq (best but slow on Android)"]

set sound_setting_menu {
	font-size 8
	border-size 2
	width 180
	xpos 100
	ypos 120
	items {{ text "Sound Settings"
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Volume: $master_volume"
	         actions { LEFT  { osd_menu::menu_setting [incr master_volume -5] }
	                   RIGHT { osd_menu::menu_setting [incr master_volume  5] }}}
	       { text "Mute: $mute"
	         actions { LEFT  { osd_menu::menu_setting [cycle_back mute] }
	                   RIGHT { osd_menu::menu_setting [cycle      mute] }}}
	       { text "Individual Sound Device Settings..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_sound_device_list]}}}
	       { text "Resampler: [osd_menu::get_resampler_presentation $resampler]"
	         actions { LEFT  { osd_menu::menu_setting [cycle_back resampler] }
	                   RIGHT { osd_menu::menu_setting [cycle      resampler] }}}}}

set horizontal_stretch_desc [dict create 320.0 "none (large borders)" 288.0 "a bit more than all border pixels" 284.0 "all border pixels" 280.0 "a bit less than all border pixels" 272.0 "realistic" 256.0 "no borders at all"]

proc menu_create_sound_device_list {} {
	set menu_def {
	         execute menu_sound_device_select_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 110
	         ypos 130
	         header { text "Select Sound Chip"
	                  font-size 10
	                  post-spacing 6 }}

	set items [machine_info sounddevice]

	return [prepare_menu_list $items 5 $menu_def]
}

proc menu_sound_device_select_exec {item} {
	menu_create [create_sound_device_settings_menu $item]
	select_menu_item $item
}

proc create_sound_device_settings_menu {device} {
	set ypos 140
	set menu_def [list \
		font-size 8 \
		border-size 2 \
		width 210 \
		xpos 120 \
		ypos $ypos]
	lappend items [list text "$device Settings" \
	         font-size 10 \
	         post-spacing 6 \
	         selectable false]

	# volume and balance
	foreach aspect [list volume balance] {
		set var_name ::${device}_${aspect}
		set item [list]
		lappend item "text"
		set first [string range $aspect 0 0]
		set rest [string range $aspect 1 end]
		set first [string toupper $first]
		set capped_aspect "${first}${rest}"
		lappend item "$capped_aspect: \[[list set $var_name]]"
		lappend item "actions"
		set actions [list]
		lappend actions "LEFT"
		lappend actions "osd_menu::menu_setting \[[list incr $var_name -5]]"
		lappend actions "RIGHT"
		lappend actions "osd_menu::menu_setting \[[list incr $var_name  5]]"
		lappend item $actions
		lappend items $item
	}
	# channel mute
	set channel_count [soundchip_utils::get_num_channels $device]
	for {set channel 1} {$channel <= $channel_count} {incr channel} {
		set chmute_var_name ${device}_ch${channel}_mute
		set item [list]
		lappend item "text"
		set pretext ""
		if {$channel_count > 1} {
			set pretext "Channel $channel "
		}
		lappend item "${pretext}Mute: \[[list set $chmute_var_name]]"
		lappend item "actions"
		set actions [list]
		lappend actions "LEFT"
		lappend actions "osd_menu::menu_setting \[[list cycle_back $chmute_var_name]]"
		lappend actions "RIGHT"
		lappend actions "osd_menu::menu_setting \[[list cycle      $chmute_var_name]]"
		lappend item $actions
		lappend items $item
	}

	# adjust menu position for longer lists
	# TODO: make this less magic
	if {$channel_count > 8} {;# more won't fit
		dict set menu_def ypos [expr {$ypos - round(($channel_count - 8) * ($ypos - 10)/16)}]
	}

	dict set menu_def items $items
	return $menu_def
}

proc create_video_setting_menu {} {
	variable scaling_available

	set menu_def {
		font-size 8
		border-size 2
		width 210
		xpos 100
		ypos 110
	}
	lappend items { text "Video Settings"
	         font-size 10
	         post-spacing 6
	         selectable false }
	if {[expr {[lindex [lindex [openmsx_info setting videosource] 2] 1] > 1}]} {
		lappend items { text "Video source: $videosource"
			actions { LEFT  { osd_menu::menu_setting [cycle_back videosource] }
			          RIGHT { osd_menu::menu_setting [cycle      videosource] }}
				  post-spacing 6}
	}
	if {$scaling_available} {
		lappend items { text "Scaler: $scale_algorithm"
			actions { LEFT  { osd_menu::menu_setting [cycle_back scale_algorithm] }
			          RIGHT { osd_menu::menu_setting [cycle      scale_algorithm] }}}
		# only add scale factor setting if it can actually be changed
		set scale_minmax [lindex [openmsx_info setting scale_factor] 2]
		if {[expr {[lindex $scale_minmax 0] != [lindex $scale_minmax 1]}]} {
			lappend items { text "Scale Factor: ${scale_factor}x"
				actions { LEFT  { osd_menu::menu_setting [incr scale_factor -1] }
				          RIGHT { osd_menu::menu_setting [incr scale_factor  1] }}}
		}
	}
	lappend items { text "Horizontal Stretch: [osd_menu::get_horizontal_stretch_presentation $horizontal_stretch]"
	         actions { A  { osd_menu::menu_create [osd_menu::menu_create_stretch_list]; osd_menu::select_menu_item $horizontal_stretch }}
	         post-spacing 6 }
	if {$scaling_available} {
		lappend items { text "Scanline: $scanline%"
			actions { LEFT  { osd_menu::menu_setting [incr scanline -1] }
			          RIGHT { osd_menu::menu_setting [incr scanline  1] }}}
		lappend items { text "Blur: $blur%"
			actions { LEFT  { osd_menu::menu_setting [incr blur -1] }
			          RIGHT { osd_menu::menu_setting [incr blur  1] }}}
	}
	if {$::renderer eq "SDLGL-PP"} {
		lappend items { text "Glow: $glow%"
			actions { LEFT  { osd_menu::menu_setting [incr glow -1] }
			          RIGHT { osd_menu::menu_setting [incr glow  1] }}}
		lappend items { text "Display Deform: $display_deform"
			actions { LEFT  { osd_menu::menu_setting [cycle_back display_deform] }
			          RIGHT { osd_menu::menu_setting [cycle      display_deform] }}}
	}
	lappend items { text "Noise: $noise%"
		actions { LEFT  { osd_menu::menu_setting [set noise [expr $noise - 1]] }
		          RIGHT { osd_menu::menu_setting [set noise [expr $noise + 1]] }}
			  post-spacing 6}
	lappend items { text "Enforce VDP Sprites-per-line Limit: $limitsprites"
			actions { LEFT  { osd_menu::menu_setting [cycle_back limitsprites] }
			          RIGHT { osd_menu::menu_setting [cycle      limitsprites] }}}
	dict set menu_def items $items
	return $menu_def
}

set hardware_menu {
	font-size 8
	border-size 2
	width 175
	xpos 100
	ypos 120
	items {{ text "Hardware"
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Change Machine..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_load_machine_list]; catch { osd_menu::select_menu_item [machine_info config_name]} }}}
	       { text "Set Current Machine as Default"
	         actions { A { set ::default_machine [machine_info config_name]; osd_menu::menu_close_top }}}
	       { text "Extensions..."
	         actions { A { osd_menu::menu_create $osd_menu::extensions_menu }}}
	       { text "Connectors..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_connectors_list] }}}
	 }}

set extensions_menu {
	font-size 8
	border-size 2
	width 175
	xpos 100
	ypos 120
	items {{ text "Extensions"
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Add..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_extensions_list] }}}
	       { text "Remove..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_plugged_extensions_list] }}}}}

set advanced_menu {
	font-size 8
	border-size 2
	width 175
	xpos 100
	ypos 120
	items {{ text "Advanced"
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Manage Running Machines..."
	         actions { A { osd_menu::menu_create $osd_menu::running_machines_menu }}}
	       { text "Toys and Utilities..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_toys_list] }}}}}

set running_machines_menu {
	font-size 8
	border-size 2
	width 175
	xpos 100
	ypos 120
	items {{ text "Manage Running Machines"
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Select Running Machine Tab: [utils::get_machine_display_name]"
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_running_machine_list] }}}
	       { text "New Running Machine Tab"
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_load_machine_list "add"] }}}
	       { text "Close Current Machine Tab"
	         actions { A { set old_active_machine [activate_machine]; cycle_machine; delete_machine $old_active_machine }}}}}

proc menu_create_running_machine_list {} {
	set menu_def {
	         execute menu_machine_tab_select_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 110
	         ypos 130
	         header { text "Select Running Machine"
	                  font-size 10
	                  post-spacing 6 }}

	set items [utils::get_ordered_machine_list]

	set presentation [list]
	foreach i $items {
		if {[activate_machine] eq $i} {
			set postfix_text "current"
		} else {
			set postfix_text [utils::get_machine_time $i]
		}
		lappend presentation [format "%s (%s)" [utils::get_machine_display_name ${i}] $postfix_text]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 5 $menu_def]
}

proc menu_machine_tab_select_exec {item} {
	menu_close_top
	activate_machine $item
}

proc get_resampler_presentation { value } {
	if {[dict exists $osd_menu::resampler_desc $value]} {
		return [dict get $osd_menu::resampler_desc $value]
	} else {
		return $value
	}
}

proc get_horizontal_stretch_presentation { value } {
	if {[dict exists $osd_menu::horizontal_stretch_desc $value]} {
		return [dict get $osd_menu::horizontal_stretch_desc $value]
	} else {
		return "custom: $::horizontal_stretch"
	}
}

proc menu_create_stretch_list {} {

	set menu_def [list \
	         execute menu_stretch_exec \
	         font-size 8 \
	         border-size 2 \
	         width 150 \
	         xpos 110 \
	         ypos 130 \
	         header { text "Select Horizontal Stretch:"
	                  font-size 10
	                  post-spacing 6 }]

	set items [list]
	set presentation [list]

	set values [dict keys $osd_menu::horizontal_stretch_desc]
	if {$::horizontal_stretch ni $values} {
		lappend values $::horizontal_stretch
	}
	foreach value $values {
		lappend items $value
		lappend presentation [osd_menu::get_horizontal_stretch_presentation $value]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 6 $menu_def]
}

proc menu_stretch_exec {value} {
	set ::horizontal_stretch $value
	menu_close_top
	menu_refresh_top
}

# Returns list of machines/extensions, but try to filter out duplicates caused
# by symlinks (e.g. turbor.xml -> Panasonic_FS-A1GT.xml). What this does not
# catch is a symlink in the systemdir (so link also pointing to the systemdir)
# and a similarly named file in the userdir. This situation does occur on my
# development setup, but it shouldn't happen for regular users.
proc get_filtered_configs {type} {
	set result [list]
	set configs [list]
	foreach t [openmsx_info $type] {
		# try both <name>.xml and <name>/hardwareconfig.xml
		set conf [data_file $type/$t.xml]
		if {![file exists $conf]} {
			set conf [data_file $type/$t/hardwareconfig.xml]
		}
		# follow symlink (on platforms that support links)
		catch {
			set conf [file join [file dirname $conf]
			                    [file readlink $conf]]
		}
		# only add if the (possibly resolved link) hasn't been seen before
		if {$conf ni $configs} {
			lappend configs $conf
			lappend result $t
		}
	}
	return $result
}

proc menu_create_load_machine_list {{mode "replace"}} {
	if {$mode eq "replace"} {
		set proc_to_exec osd_menu::menu_load_machine_exec_replace
	} elseif {$mode eq "add"} {
		set proc_to_exec osd_menu::menu_load_machine_exec_add
	} else {
		error "Undefined mode: $mode"
	}

	set menu_def [list \
	         execute $proc_to_exec \
	         font-size 8 \
	         border-size 2 \
	         width 200 \
	         xpos 110 \
	         ypos 130 \
	         header { text "Select Machine to Run"
	                  font-size 10
	                  post-spacing 6 }]

	set items [get_filtered_configs machines]

	foreach i $items {
		set extra_info ""
		if {$i eq $::default_machine} {
			set extra_info " (default)"
		}
		lappend presentation "[utils::get_machine_display_name_by_config_name $i]$extra_info"
	}

	set items_sorted [list]
	set presentation_sorted [list]

	foreach i [lsort -dictionary -indices $presentation] {
		lappend presentation_sorted [lindex $presentation $i]
		lappend items_sorted [lindex $items $i]
	}

	lappend menu_def presentation $presentation_sorted
	return [prepare_menu_list $items_sorted 10 $menu_def]
}

proc menu_load_machine_exec_replace {item} {
	if {[catch {machine $item} errorText]} {
		osd::display_message $errorText error
	} else {
		menu_close_all
	}
}

proc menu_load_machine_exec_add {item} {
	set id [create_machine]
	set err [catch {${id}::load_machine $item} error_result]
	if {$err} {
		delete_machine $id
		osd::display_message "Error starting [utils::get_machine_display_name_by_config_name $item]: $error_result" error
	} else {
		menu_close_top
		activate_machine $id
	}
}

proc menu_create_extensions_list {} {
	set menu_def {
	         execute menu_add_extension_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 110
	         ypos 130
	         header { text "Select Extension to Add"
	                  font-size 10
	                  post-spacing 6 }}

	set items [get_filtered_configs extensions]
	set presentation [list]

	foreach i $items {
		lappend presentation [utils::get_extension_display_name_by_config_name $i]
	}

	set items_sorted [list]
	set presentation_sorted [list]

	foreach i [lsort -dictionary -indices $presentation] {
		lappend presentation_sorted [lindex $presentation $i]
		lappend items_sorted [lindex $items $i]
	}

	lappend menu_def presentation $presentation_sorted

	return [prepare_menu_list $items_sorted 10 $menu_def]
}

proc menu_add_extension_exec {item} {
	if {[catch {ext $item} errorText]} {
		osd::display_message $errorText error
	} else {
		menu_close_all
	}
}

proc menu_create_plugged_extensions_list {} {
	set menu_def {
	         execute menu_remove_extension_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 110
	         ypos 130
	         header { text "Select Extension to Remove"
	                  font-size 10
	                  post-spacing 6 }}

	set items [list_extensions]
	set possible_items [get_filtered_configs extensions]

	set useful_items [list]
	foreach item $items {
		if {$item in $possible_items} {
			lappend useful_items $item
		}
	}

	set presentation [list]

	foreach i $useful_items {
		lappend presentation [utils::get_extension_display_name_by_config_name ${i}]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $useful_items 10 $menu_def]
}

proc menu_remove_extension_exec {item} {
	menu_close_all
	remove_extension $item
}

proc menu_create_connectors_list {} {
	set menu_def {
	         execute menu_connector_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         header { text "Connectors"
	                  font-size 10
	                  post-spacing 6 }}

	set items [machine_info connector]

	set presentation [list]
	foreach item $items {
		set plugged [get_pluggable_for_connector $item]
		set plugged_presentation ""
		if {$plugged ne ""} {
			set plugged_presentation "  ([machine_info pluggable $plugged])"
		}
		lappend presentation "[machine_info connector $item]: $plugged$plugged_presentation"
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 5 $menu_def]
}

proc menu_connector_exec {item} {
	menu_create [create_menu_pluggable_list $item]
	select_menu_item [get_pluggable_for_connector $item]
}

proc create_menu_pluggable_list {connector} {
	set menu_def [list \
	         execute [list menu_plug_exec $connector] \
	         font-size 8 \
	         border-size 2 \
	         width 200 \
	         xpos 110 \
	         ypos 140 \
	         header [list text "What to Plug into [machine_info connector $connector]?" \
	                  font-size 10 \
	                  post-spacing 6 ]]

	set items [list]

	set class [machine_info connectionclass $connector]

	# find out which pluggables are already plugged
	# (currently a pluggable can be used only once per machine)
	set already_plugged [list]
	foreach other_connector [machine_info connector] {
		set other_plugged [get_pluggable_for_connector $other_connector]
		if {$other_plugged ne "" && $other_connector ne $connector} {
			lappend already_plugged $other_plugged
		}
	}

	# get a list of all pluggables that fit this connector
	# and which are not plugged yet in other connectors
	foreach pluggable [machine_info pluggable] {
		if {$pluggable ni $already_plugged && [machine_info connectionclass $pluggable] eq $class} {
			lappend items $pluggable
		}
	}

	set presentation [list]
	foreach item $items {
		lappend presentation "$item: [machine_info pluggable $item]"
	}

	set plugged [get_pluggable_for_connector $connector]
	if {$plugged ne ""} {
		set items [linsert $items 0 "--unplug--"]
		set presentation [linsert $presentation 0 "Nothing, unplug $plugged ([machine_info pluggable $plugged])"]
	}

	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 5 $menu_def]
}

proc menu_plug_exec {connector pluggable} {
	set command ""
	if {$pluggable eq "--unplug--"} {
		set command "unplug {$connector}"
	} else {
		set command "plug {$connector} {$pluggable}"
	}
	#note: NO braces around $command
	if {[catch $command errorText]} {
		osd::display_message $errorText error
	} else {
		menu_close_top
		# refresh the connectors menu
		# The list must be recreated, so menu_refresh_top won't work
		menu_close_top
		menu_create [menu_create_connectors_list]
	}
}

proc menu_create_toys_list {} {
	set menu_def {
	         execute menu_toys_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         header { text "Toys and Utilities"
	                  font-size 10
	                  post-spacing 6 }}

	set items [list]
	set presentation [list]
	# This also picks up 'lazy' command names
	foreach cmd [openmsx::all_command_names] {
		if {[string match toggle_* $cmd]} {
			lappend items $cmd
			lappend presentation [string map {_ " "} [string range $cmd 7 end]]
		}
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 5 $menu_def]
}

proc menu_toys_exec {toy} {
	return [$toy]
}

proc ls {directory extensions} {
	set dirs [list]
	set specialdir [list]
	set items [list]
	if {[catch {
		set files [glob -nocomplain -tails -directory $directory -types {f r} *]
		set items [lsearch -regexp -all -inline -nocase $files .*\\.($extensions)]
		set dirs [glob -nocomplain -tails -directory $directory -types {d r x} *]
		set specialdir [glob -nocomplain -tails -directory $directory -types {hidden d} ".openMSX"]
	} errorText]} {
		osd::display_message "Unable to read dir $directory: $errorText" error
	}
	set dirs2 [list]
	foreach dir [concat $dirs $specialdir] {
		lappend dirs2 "$dir/"
	}
	set extra_entries [list]
	set volumes [file volumes]
	if {$directory ni $volumes} {
		# check whether .. is readable (it's not always so on Android)
		if {[file readable [file join $directory ..]]} {
			lappend extra_entries ".."
		}
	} else {
		if {[llength $volumes] > 1} {
			set extra_entries $volumes
		}
	}
	return [concat [lsort $extra_entries] [lsort $dirs2] [lsort $items]]
}

proc is_empty_dir {directory extensions} {
	set files [list]
	catch {set files [glob -nocomplain -tails -directory $directory -types {f r} *]}
	set items [lsearch -regexp -all -inline -nocase $files .*\\.($extensions)]
	if {[llength $items] != 0} {return false}

	set dirs [list]
	catch {set dirs [glob -nocomplain -tails -directory $directory -types {d r x} *]}
	if {[llength $dirs] != 0} {return false}

	set specialdir [list]
	catch {set specialdir [glob -nocomplain -tails -directory $directory -types {hidden d} ".openMSX"]}
	if {[llength $specialdir] != 0} {return false}

	return true
}

proc menu_create_ROM_list {path slot} {
	set menu_def [list execute [list menu_select_rom $slot] \
		font-size 8 \
		border-size 2 \
		width 200 \
		xpos 100 \
		ypos 120 \
		header { text "ROMs $::osd_rom_path" \
			font-size 10 \
			post-spacing 6 }]
	set extensions "rom|ri|mx1|mx2|zip|gz"
	set items [list]
	set presentation [list]
	if {[lindex [$slot] 2] ne "empty"} {
		lappend items "--eject--"
		lappend presentation "--eject-- [file tail [lindex [$slot] 1]]"
	}
	set i 1
	foreach pool_path [filepool::get_paths_for_type rom] {
		if {$path ne $pool_path && [file exists $pool_path] &&
		    ![is_empty_dir $pool_path $extensions]} {
			lappend items $pool_path
			lappend presentation "\[ROM Pool $i\]"
		}
		incr i
	}
	set files [ls $path $extensions]
	set items [concat $items $files]
	set presentation [concat $presentation $files]

	lappend menu_def presentation $presentation
	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_select_rom {slot item} {
	if {$item eq "--eject--"} {
		menu_close_all
		$slot eject
		reset
	} else {
		set fullname [file join $::osd_rom_path $item]
		if {[file isdirectory $fullname]} {
			menu_close_top
			set ::osd_rom_path [file normalize $fullname]
			menu_create [menu_create_ROM_list $::osd_rom_path $slot]
		} else {
			set mappertype ""
			set hash [sha1sum $fullname]
			if {[catch {set mappertype [dict get [openmsx_info software $hash] mapper_type_name]}]} {
				# not in the database, execute after selecting mapper type
				menu_create [menu_create_mappertype_list $slot $fullname]
			} else {
				# in the database, so just execute
				menu_rom_with_mappertype_exec $slot $fullname $mappertype
			}
		}
	}
}

proc menu_rom_with_mappertype_exec {slot fullname mappertype} {
	if {[catch {$slot $fullname -romtype $mappertype} errorText]} {
		osd::display_message "Can't insert ROM: $errorText" error
	} else {
		menu_close_all

		set rominfo [getlist_rom_info]

		if {$rominfo eq ""} {
			osd::display_message "No ROM information available..."
		} else {
			osd::display_message "Now running ROM:\nTitle:\nYear:\nCompany:\nCountry:\nStatus:\nRemark:"

			append result " \n" \
						  "[dict get $rominfo title]\n" \
						  "[dict get $rominfo year]\n" \
						  "[dict get $rominfo company]\n" \
						  "[dict get $rominfo country]\n" \
						  "[dict get $rominfo status]\n"

			if {[dict get $rominfo remark] ne ""} {
				append result [dict get $rominfo remark]
			} else {
				append result "None"
			}

			set txt_size 6
			set xpos 35

			# TODO: prevent this from being duplicated from osd_widgets::text_box
			if {$::scale_factor == 1} {
				set txt_size 9
				set xpos 53
			}

			# TODO: this code knows the internal name of the widget of osd::display_message proc... it shouldn't need to.
			osd create text osd_display_message.rominfo_text -x $xpos -y 2 -size $txt_size -rgba 0xffffffff -text "$result"
		}
		reset
	}
}

proc menu_create_mappertype_list {slot fullname} {
	set menu_def [list execute [list menu_rom_with_mappertype_exec $slot $fullname] \
	         font-size 8 \
	         border-size 2 \
	         width 200 \
	         xpos 100 \
	         ypos 120 \
	         header { text "Select mapper type" \
	                  font-size 10 \
	                  post-spacing 6 }]

	set items [openmsx_info romtype]
	set presentation [list]

	foreach i $items {
		lappend presentation "[dict get [openmsx_info romtype $i] description]"
	}

	set items_sorted [list "auto"]
	set presentation_sorted [list "Auto-detect (guess)"]

	foreach i [lsort -dictionary -indices $presentation] {
		lappend presentation_sorted [lindex $presentation $i]
		lappend items_sorted [lindex $items $i]
	}

	lappend menu_def presentation $presentation_sorted
	return [prepare_menu_list $items_sorted 10 $menu_def]
}

proc menu_create_disk_list {path drive} {
	set menu_def [list execute [list menu_select_disk $drive] \
		font-size 8 \
		border-size 2 \
		width 200 \
		xpos 100 \
		ypos 120 \
		header { text "Disks $::osd_disk_path" \
			font-size 10 \
			post-spacing 6 }]
	set cur_image [lindex [$drive] 1]
	set extensions "dsk|zip|gz|xsa|dmk|di1|di2"
	set items [list]
	set presentation [list]
	if {[lindex [$drive] 2] ne "empty readonly"} {
		lappend items "--eject--"
		lappend presentation "--eject-- [file tail $cur_image]"
	}
	set i 1
	foreach pool_path [filepool::get_paths_for_type disk] {
		if {$path ne $pool_path && [file exists $pool_path] &&
		    ![is_empty_dir $pool_path $extensions]} {
			lappend items $pool_path
			lappend presentation "\[Disk Pool $i\]"
		}
		incr i
	}

	if {$cur_image ne $path} {
		lappend items "."
		lappend presentation "--insert this dir as disk--"
	}

	set files [ls $path $extensions]
	set items [concat $items $files]
	set presentation [concat $presentation $files]

	lappend menu_def presentation $presentation
	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_select_disk {drive item} {
	if {$item eq "--eject--"} {
		set cur_image [lindex [$drive] 1]
		menu_close_all
		$drive eject
		osd::display_message "Disk $cur_image ejected!"
	} else {
		set fullname [file normalize [file join $::osd_disk_path $item]]
		if {[file isdirectory $fullname] && $item ne "."} {
			menu_close_top
			set ::osd_disk_path [file normalize $fullname]
			menu_create [menu_create_disk_list $::osd_disk_path $drive]
		} else {
			if {[catch {$drive $fullname} errorText]} {
				osd::display_message "Can't insert disk: $errorText" error
			} else {
				menu_close_all
				if {$item eq "."} { set item $fullname }
				osd::display_message "Disk $item inserted!"
			}
		}
	}
}

proc menu_create_tape_list {path} {
	variable taperecordings_directory

	set menu_def { execute menu_select_tape
		font-size 8
		border-size 2
		width 200
		xpos 100
		ypos 120
		header { text "Tapes $::osd_tape_path"
			font-size 10
			post-spacing 6 }}
	set extensions "cas|wav|zip|gz"

	set items [list]
	set presentation [list]
	lappend items "--create--"
	lappend presentation "--create new and insert--"
	set inserted [lindex [cassetteplayer] 1]
	if {$inserted ne ""} {
		lappend items "--eject--"
		lappend presentation "--eject-- [file tail $inserted]"
		lappend items "--rewind-"
		lappend presentation "--rewind-- [file tail $inserted]"
	}
	if {$path ne $taperecordings_directory && [file exists $taperecordings_directory]} {
		lappend items $taperecordings_directory
		lappend presentation "\[My Tape Recordings\]"
	}
	set i 1
	foreach pool_path [filepool::get_paths_for_type tape] {
		if {$path ne $pool_path && [file exists $pool_path] &&
		    ![is_empty_dir $pool_path $extensions]} {
			lappend items $pool_path
			lappend presentation "\[Tape Pool $i\]"
		}
		incr i
	}

	set files [ls $path $extensions]
	set items [concat $items $files]
	set presentation [concat $presentation $files]

	lappend menu_def presentation $presentation
	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_select_tape {item} {
	variable taperecordings_directory
	if {$item eq "--create--"} {
		menu_close_all
		osd::display_message [cassetteplayer new [menu_free_tape_name]]
	} elseif {$item eq "--eject--"} {
		menu_close_all
		osd::display_message [cassetteplayer eject]
	} elseif {$item eq "--rewind--"} {
		menu_close_all
		osd::display_message [cassetteplayer rewind]
	} else {
		set fullname [file join $::osd_tape_path $item]
		if {[file isdirectory $fullname]} {
			menu_close_top
			set ::osd_tape_path [file normalize $fullname]
			menu_create [menu_create_tape_list $::osd_tape_path]
		} else {
			if {[catch {cassetteplayer $fullname} errorText]} {
				osd::display_message "Can't set tape: $errorText" error
			} else {
				osd::display_message "Inserted tape $item!"
				menu_close_all
			}
		}
	}
}

proc menu_free_tape_name {} {
	variable taperecordings_directory
	set existing [list]
	foreach f [lsort [glob -tails -directory $taperecordings_directory -type f -nocomplain *.wav]] {
		lappend existing [file rootname $f]
	}
	set i 1
	while 1 {
		set name [format "[guess_title untitled] %04d" $i]
		if {$name ni $existing} {
			return $name
		}
		incr i
	}
}

proc menu_create_hdd_list {path drive} {
	return [prepare_menu_list [ls $path "dsk|zip|gz|hdd"] \
	                          10 \
	                          [list execute [list menu_select_hdd $drive]\
	                            font-size 8 \
	                            border-size 2 \
	                            width 200 \
	                            xpos 100 \
	                            ypos 120 \
	                            header { text "Hard disk images $::osd_hdd_path"
	                                     font-size 10
	                                     post-spacing 6 }]]
}

proc menu_select_hdd {drive item} {
	set fullname [file join $::osd_hdd_path $item]
	if {[file isdirectory $fullname]} {
		menu_close_top
		set ::osd_hdd_path [file normalize $fullname]
		menu_create [menu_create_hdd_list $::osd_hdd_path $drive]
	} else {
		confirm_action "Really power off to change HDD image?" osd_menu::confirm_change_hdd [list $item $drive]
	}
}

proc confirm_change_hdd {item result} {
	menu_close_top
	if {$result eq "Yes"} {
		set fullname [file join $::osd_hdd_path [lindex $item 0]]
		if {[catch {set ::power off; [lindex $item 1] $fullname} errorText]} {
			osd::display_message "Can't change hard disk image: $errorText" error
			# TODO: we already powered off even though the file may be invalid... save state first?
		} else {
			osd::display_message "Changed hard disk image to [lindex $item 0]!"
			menu_close_all
		}
		set ::power on
	}
}

proc menu_create_ld_list {path} {
	set eject_item [list]
	set inserted [lindex [laserdiscplayer] 1]
	if {$inserted ne ""} {
		lappend eject_item "--eject-- [file tail $inserted]"
	}
	return [prepare_menu_list [concat $eject_item [ls $path "ogv"]] \
	                          10 \
	                          { execute menu_select_ld
	                            font-size 8
	                            border-size 2
	                            width 200
	                            xpos 100
	                            ypos 120
	                            header { text "Laserdiscs $::osd_ld_path"
	                                     font-size 10
	                                     post-spacing 6 }}]
}

proc menu_select_ld {item} {
	if {[string range $item 0 8] eq "--eject--"} {
		menu_close_all
		osd::display_message [laserdiscplayer eject]
	} else {
		set fullname [file join $::osd_ld_path $item]
		if {[file isdirectory $fullname]} {
			menu_close_top
			set ::osd_ld_path [file normalize $fullname]
			menu_create [menu_create_ld_list $::osd_ld_path]
		} else {
			if {[catch {laserdiscplayer insert $fullname} errorText]} {
				osd::display_message "Can't load LaserDisc: $errorText" error
			} else {
				osd::display_message "Loaded LaserDisc $item!"
				menu_close_all
			}
		}
	}
}

proc get_savestates_list_presentation_sorted {} {
	set presentation [list]
	foreach i [lsort -integer -index 1 -decreasing [savestate::list_savestates_raw]] {
		if {[info commands clock] ne ""} {
			set pres_str [format "%s (%s)" [lindex $i 0] [clock format [lindex $i 1] -format "%x - %X"]]
		} else {
			set pres_str [lindex $i 0]
		}
		lappend presentation $pres_str
	}
	return $presentation
}

proc menu_create_load_state {} {
	set menu_def \
	       { execute menu_loadstate_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         on-open  {osd create rectangle "preview" -x 225 -y 5 -w 90 -h 70 -rgba 0x30303080 -scaled true}
	         on-close {osd destroy "preview"}
	         on-select   menu_loadstate_select
	         on-deselect menu_loadstate_deselect
	         header { text "Load State"
	                  font-size 10
	                  post-spacing 6 }}

	set items [list_savestates -t]

	lappend menu_def presentation [get_savestates_list_presentation_sorted]

	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_create_save_state {} {
	set items [concat [list "create new"] [list_savestates -t]]
	set menu_def \
	       { execute menu_savestate_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         on-open  {osd create rectangle "preview" -x 225 -y 5 -w 90 -h 70 -rgba 0x30303080 -scaled true}
	         on-close {osd destroy "preview"}
	         on-select   menu_loadstate_select
	         on-deselect menu_loadstate_deselect
	         header { text "Save State"
	                  font-size 10
	                  post-spacing 6 }}


	lappend menu_def presentation [concat [list "create new"] [get_savestates_list_presentation_sorted]]

	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_loadstate_select {item} {
	set png $::env(OPENMSX_USER_DATA)/../savestates/${item}.png
	catch {osd create rectangle "preview.image" -relx 0.05 -rely 0.05 -w 80 -h 60 -image $png}
}

proc menu_loadstate_deselect {item} {
	osd destroy "preview.image"
}

proc menu_loadstate_exec {item} {
	if {[catch {loadstate $item} errorText]} {
		osd::display_message $errorText error
	} else {
		menu_close_all
	}
}

proc menu_savestate_exec {item} {
	if {$item eq "create new"} {
		set item [menu_free_savestate_name]
		confirm_save_state $item "Yes"
		menu_close_all
	} else {
		confirm_action "Overwrite $item?" osd_menu::confirm_save_state $item
	}
}

proc confirm_save_state {item result} {
	menu_close_top
	if {$result eq "Yes"} {
		if {[catch {savestate $item} errorText]} {
			osd::display_message $errorText error
		} else {
			osd::display_message "State saved to $item!"
			menu_close_all
		}
	}
}

proc menu_free_savestate_name {} {
	set existing [list_savestates]
	set i 1
	while 1 {
		set name [format "[guess_title savestate] %04d" $i]
		if {$name ni $existing} {
			return $name
		}
		incr i
	}
}

proc confirm_action {text action item} {
	set items [list "No" "Yes"]
	set menu_def [list execute [list $action $item] \
			font-size 8 \
			border-size 2 \
			width 210 \
			xpos 100 \
			ypos 100 \
			header [list text $text \
					font-size 10 \
					post-spacing 6 ]]

	osd_menu::menu_create [osd_menu::prepare_menu_list $items [llength $items] $menu_def]
}

# Keep openmsx console from interfering with the osd menu:
#  when the console is activated while the osd menu is already open, we want
#  to prevent the osd menu from receiving the keys that are pressed in the
#  console.
variable old_console $::console
proc console_input_layer {name1 name1 op} {
	global console
	variable old_console
	if {$console == $old_console} return
	set old_console $console
	if {$console} {
		activate_input_layer console -blocking
	} else {
		deactivate_input_layer console
	}
}
trace add variable ::console write [namespace code console_input_layer]

namespace export main_menu_open
namespace export main_menu_close
namespace export main_menu_toggle

} ;# namespace osd_menu

namespace import osd_menu::*
