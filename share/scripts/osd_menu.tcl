namespace eval osd_menu {

set_help_text main_menu_open   "(experimental) Show the OSD menu."
set_help_text main_menu_close  "(experimental) Remove the OSD menu."
set_help_text main_menu_toggle "(experimental) Toggle the OSD menu."

# default colors defined here, for easy global tweaking
variable default_bg_color "0x7090aae8 0xa0c0dde8 0x90b0cce8 0xc0e0ffe8"
variable default_text_color 0x000000ff
variable default_text_color 0x000000ff
variable default_select_color "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80"
variable default_header_text_color 0xff9020ff
variable error_color "0xaa0000e8 0xdd0000e8 0xcc0000e8 0xff0000e8"
variable warning_color "0xaa6600e8 0xdd9900e8 0xcc8800e8 0xffaa00e8"

variable is_dingoo [string match *-dingux* $::tcl_platform(osVersion)]

proc get_optional { array_name key default } {
	upvar $array_name arr
	if [info exists arr($key)] {
		return $arr($key)
	} else {
		return $default
	}
}

proc set_optional { array_name key value } {
	upvar $array_name arr
	if ![info exists arr($key)] {
		set arr($key) $value
	}
}

variable menulevels 0
variable main_menu

proc push_menu_info {} {
	variable menulevels
	incr menulevels 1
	set levelname "menuinfo_$menulevels"
	variable $levelname
	array set $levelname [uplevel { list name $name lst $lst menu_len $menu_len presentation $presentation menutexts $menutexts selectinfo $selectinfo selectidx $selectidx scrollidx $scrollidx on_close $on_close }]
}

proc peek_menu_info {} {
	variable menulevels
	uplevel upvar #0 osd_menu::menuinfo_$menulevels menuinfo
}

proc set_selectidx { value } {
	peek_menu_info
	array set menuinfo [list selectidx $value]
}

proc set_scrollidx { value } {
	peek_menu_info
	array set menuinfo [list scrollidx $value]
}

proc menu_create { menu_def_list } {
	variable menulevels
	variable default_bg_color
	variable default_text_color
	variable default_select_color
	variable default_header_text_color

	set name "menu[expr $menulevels + 1]"

	array set menudef $menu_def_list

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
	set items $menudef(items)
	foreach itemdef $items {
		array unset itemarr
		array set itemarr $itemdef
		set selectable [get_optional itemarr "selectable" true]
		incr y [get_optional itemarr "pre-spacing" 0]
		set fontsize  [get_optional itemarr "font-size"  $deffontsize]
		set font      [get_optional itemarr "font"       $deffont]
		if {$selectable} {
			set textcolor [get_optional itemarr "text-color" $deftextcolor]
		} else {
			set textcolor [get_optional itemarr "text-color" $default_header_text_color]
		}
		set actions     [get_optional itemarr "actions"     ""]
		set on_select   [get_optional itemarr "on-select"   ""]
		set on_deselect [get_optional itemarr "on-deselect" ""]
		set textid "${name}.item${y}"
		set text $itemarr(text)
		lappend menutexts $textid $text
		osd create text $textid -font $font -size $fontsize \
					-rgba $textcolor -x $bordersize -y $y
		if $selectable {
			set allactions [concat $defactions $actions]
			lappend selectinfo [list $y $fontsize $allactions $on_select $on_deselect]
		}
		incr y $fontsize
		incr y [get_optional itemarr "post-spacing" 0]
	}

	set width $menudef(width)
	set height [expr $y + $bordersize]
	set xpos [get_optional menudef "xpos" [expr (320 - $width)  / 2]]
	set ypos [get_optional menudef "ypos" [expr (240 - $height) / 2]]
	osd configure $name -x $xpos -y $ypos -w $width -h $height

	set selw [expr $width - 2 * $bordersize]
	osd create rectangle "${name}.selection" -z -1 -rgba $selectcolor \
		-x $bordersize -w $selw

	set lst [get_optional menudef "lst" ""]
	set menu_len [get_optional menudef "menu_len" 0]
	set presentation [get_optional menudef "presentation" ""]
	set selectidx 0
	set scrollidx 0
	push_menu_info

	uplevel #0 $on_open
	menu_on_select $selectinfo $selectidx

	menu_refresh_top
}

proc menu_refresh_top {} {
	peek_menu_info
	foreach { osdid text } $menuinfo(menutexts) {
		set cmd [list subst $text]
		osd configure $osdid -text [uplevel #0 $cmd]
	}

	if {[llength $menuinfo(selectinfo)] == 0} return
	set sely [lindex $menuinfo(selectinfo) $menuinfo(selectidx) 0]
	set selh [lindex $menuinfo(selectinfo) $menuinfo(selectidx) 1]
	osd configure "$menuinfo(name).selection" -y $sely -h $selh
}

proc menu_close_top {} {
	variable menulevels
	peek_menu_info
	menu_on_deselect $menuinfo(selectinfo) $menuinfo(selectidx)
	uplevel #0 $menuinfo(on_close)
	osd destroy $menuinfo(name)
	array unset menuinfo
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

proc menu_setting { cmd_result } {
	menu_refresh_top
}

proc menu_updown { delta } {
	peek_menu_info
	set num [llength $menuinfo(selectinfo)]
	if {$num == 0} return

	menu_on_deselect $menuinfo(selectinfo) $menuinfo(selectidx)
	set_selectidx [expr ($menuinfo(selectidx) + $delta) % $num]
	menu_on_select $menuinfo(selectinfo) $menuinfo(selectidx)
	menu_refresh_top
}

proc menu_on_select { selectinfo selectidx } {
	set on_select [lindex $selectinfo $selectidx 3]
	uplevel #0 $on_select
}

proc menu_on_deselect { selectinfo selectidx } {
	set on_deselect [lindex $selectinfo $selectidx 4]
	uplevel #0 $on_deselect
}

proc menu_action { button } {
	peek_menu_info
	array set actions [lindex $menuinfo(selectinfo) $menuinfo(selectidx) 2]
	set_optional actions UP   { osd_menu::menu_updown -1 }
	set_optional actions DOWN { osd_menu::menu_updown  1 }
	set_optional actions B    { osd_menu::menu_close_top }
	set cmd [get_optional actions $button ""]
	uplevel #0 $cmd
}

user_setting create string osd_rom_path "OSD Rom Load Menu Last Known Path" $env(HOME)
user_setting create string osd_disk_path "OSD Disk Load Menu Last Known Path" $env(HOME)
if ![file exists $::osd_rom_path] {
	# revert to default (should always exist)
	unset ::osd_rom_path
}

if ![file exists $::osd_disk_path] {
	# revert to default (should always exist)
	unset ::osd_disk_path
}

proc main_menu_open {} {
	variable main_menu
	do_menu_open $main_menu
}

proc do_menu_open { top_menu } {
	variable is_dingoo

	# close console, because the menu interferes with it
	set ::console off

	# also remove other OSD controlled widgets (like the osd keyboard)
	if {[info exists ::osd_control::close]} {
		eval $::osd_control::close
	}
	# end tell how to close this widget
	namespace eval ::osd_control { set close ::osd_menu::main_menu_close }

	menu_create $top_menu

	set ::pause true
	# TODO make these bindings easier to customize
	bind_default "keyb UP"     -repeat { osd_menu::menu_action UP    }
	bind_default "keyb DOWN"   -repeat { osd_menu::menu_action DOWN  }
	bind_default "keyb LEFT"   -repeat { osd_menu::menu_action LEFT  }
	bind_default "keyb RIGHT"  -repeat { osd_menu::menu_action RIGHT }
	if {$is_dingoo} {
		bind_default "keyb LCTRL"  { osd_menu::menu_action A     }
		bind_default "keyb LALT"   { osd_menu::menu_action B     }
	} else {
		bind_default "keyb SPACE"  { osd_menu::menu_action A     }
		bind_default "keyb RETURN" { osd_menu::menu_action A     }
		bind_default "keyb ESCAPE" { osd_menu::menu_action B     }
	}
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
	variable is_dingoo

	set ::pause false
	# TODO avoid duplication with 'main_menu_open'
	unbind_default "keyb UP"
	unbind_default "keyb DOWN"
	unbind_default "keyb LEFT"
	unbind_default "keyb RIGHT"
	if {$is_dingoo} {
		unbind_default "keyb LCTRL"
		unbind_default "keyb LALT"
	} else {
		unbind_default "keyb SPACE"
		unbind_default "keyb RETURN"
		unbind_default "keyb ESCAPE"
	}

	namespace eval ::osd_control { unset close }
}

proc prepare_menu { menu_def_list } {
	# In the past we added default stuff here (like default actions
	# for UP, DOWN, B). Maybe remove this proc completely in the future.
	return $menu_def_list
}

proc prepare_menu_list { lst num menu_def_list } {
	array set menudef $menu_def_list
	set execute $menudef(execute)
	set header $menudef(header)
	set item_extra   [get_optional menudef item ""]
	set on_select    [get_optional menudef on-select ""]
	set on_deselect  [get_optional menudef on-deselect ""]
	set presentation [get_optional menudef presentation $lst]
	# 'assert': presentation should have same length as item list!
	if { [llength $presentation] != [llength $lst]} {
		error "Presentation should be of same length as item list!"
	}
	set menudef(presentation) $presentation
	lappend header "selectable" "false"
	set items [list $header]
	set lst_len [llength $lst]
	set menu_len [expr $lst_len < $num ? $lst_len : $num]
	for {set i 0} {$i < $menu_len} {incr i} {
		set actions [list "A" "osd_menu::list_menu_item_exec $execute $i"]
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
		if {$on_select != ""} {
			lappend item "on-select" "osd_menu::list_menu_item_select $i $on_select"
		}
		if {$on_deselect != ""} {
			lappend item "on-deselect" "osd_menu::list_menu_item_select $i $on_deselect"
		}
		lappend items [concat $item $item_extra]
	}
	set menudef(items) $items
	set menudef(lst) $lst
	set menudef(menu_len) $menu_len
	return [prepare_menu [array get menudef]]
}

proc list_menu_item_exec { execute pos } {
	peek_menu_info
	$execute [lindex $menuinfo(lst) [expr $pos + $menuinfo(scrollidx)]]
}

proc list_menu_item_show { pos } {
	peek_menu_info
	return [lindex $menuinfo(presentation) [expr $pos + $menuinfo(scrollidx)]]
}

proc list_menu_item_select { pos select_proc } {
	peek_menu_info
	$select_proc [lindex $menuinfo(lst) [expr $pos + $menuinfo(scrollidx)]]
}

proc move_selection { delta } {
	peek_menu_info
	set lst_last [expr [llength $menuinfo(lst)] - 1]
	set scrollidx $menuinfo(scrollidx)
	set selectidx $menuinfo(selectidx)

	set old_itemidx [expr $scrollidx + $selectidx]
	set new_itemidx [expr $old_itemidx + $delta]

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

proc select_menu_idx { itemidx } {
	peek_menu_info
	set menu_len $menuinfo(menu_len)
	set scrollidx $menuinfo(scrollidx)
	set selectidx $menuinfo(selectidx)

	menu_on_deselect $menuinfo(selectinfo) $selectidx

	set selectidx [expr $itemidx - $scrollidx]
	if {$selectidx < 0} {
		incr scrollidx $selectidx
		set selectidx 0
	} elseif {$selectidx >= $menu_len} {
		set selectidx [expr $menu_len - 1]
		set scrollidx [expr $itemidx - $selectidx]
	}

	set_selectidx $selectidx
	set_scrollidx $scrollidx
	menu_on_select $menuinfo(selectinfo) $selectidx
	menu_refresh_top
}

proc select_menu_item { item } {
	peek_menu_info

	set index [lsearch -exact $menuinfo(lst) $item]
	if {$index == -1} return

	select_menu_idx $index
}

#
# definitions of menus
#

set main_menu [prepare_menu {
	font-size 10
	border-size 2
	width 160
	items {{ text "[openmsx_info version]"
	         font-size 12
	         post-spacing 6
	         selectable false }
	       { text "Load ROM..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_ROM_list $::osd_rom_path] }}}
	       { text "Insert Disk..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_disk_list $::osd_disk_path] }}
	         post-spacing 3 }
	       { text "Save State..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_save_state] }}}
	       { text "Load State..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_load_state] }}
	         post-spacing 3 }
	       { text "Hardware..."
	         actions { A { osd_menu::menu_create $osd_menu::hardware_menu }}
	         post-spacing 3 }
	       { text "Misc Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::misc_setting_menu }}}
	       { text "Sound Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::sound_setting_menu }}}
	       { text "Video Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::video_setting_menu }}
	         post-spacing 3 }
	       { text "Advanced..."
	         actions { A { osd_menu::menu_create $osd_menu::advanced_menu }}
	         post-spacing 10 }
	       { text "Reset MSX"
	         actions { A { reset ; osd_menu::menu_close_all }}}
	       { text "Exit openMSX"
	         actions { A exit }}}}]

set misc_setting_menu [prepare_menu {
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
	                   RIGHT { osd_menu::menu_setting [incr maxframeskip  1] }}}}}]

set sound_setting_menu [prepare_menu {
	font-size 8
	border-size 2
	width 150
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
	                   RIGHT { osd_menu::menu_setting [cycle      mute] }}}}}]

set video_setting_menu [prepare_menu {
	font-size 8
	border-size 2
	width 150
	xpos 100
	ypos 120
	items {{ text "Video Settings"
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Scaler: $scale_algorithm"
	         actions { LEFT  { osd_menu::menu_setting [cycle_back scale_algorithm] }
	                   RIGHT { osd_menu::menu_setting [cycle      scale_algorithm] }}}
	       { text "Scale Factor: $scale_factor X"
	         actions { LEFT  { osd_menu::menu_setting [incr scale_factor -1] }
	                   RIGHT { osd_menu::menu_setting [incr scale_factor  1] }}
	         post-spacing 6 }
	       { text "Scanline: $scanline"
	         actions { LEFT  { osd_menu::menu_setting [incr scanline -1] }
	                   RIGHT { osd_menu::menu_setting [incr scanline  1] }}}
	       { text "Blur: $blur"
	         actions { LEFT  { osd_menu::menu_setting [incr blur -1] }
	                   RIGHT { osd_menu::menu_setting [incr blur  1] }}}
	       { text "Glow: $glow"
	         actions { LEFT  { osd_menu::menu_setting [incr glow -1] }
	                   RIGHT { osd_menu::menu_setting [incr glow  1] }}}}}]

set hardware_menu [prepare_menu {
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
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_load_machine_list] }}}
	       { text "Extensions..."
	         actions { A { osd_menu::menu_create $osd_menu::extensions_menu }}}}}]

set extensions_menu [prepare_menu {
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
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_plugged_extensions_list] }}}}}]

set advanced_menu [prepare_menu {
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
	       { text "Toys..."
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_toys_list] }}}}}]

set running_machines_menu [prepare_menu {
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
	         actions { A { set old_active_machine [activate_machine]; cycle_machine; delete_machine $old_active_machine }}}}}]

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
		if { [activate_machine] == $i } {
			set postfix_text "current"
		} else {
			set postfix_text [utils::get_machine_time $i]
		}
		lappend presentation [format "%s (%s)" [utils::get_machine_display_name ${i}] $postfix_text]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 5 $menu_def]
}

proc menu_machine_tab_select_exec { item } {
	menu_close_top
	activate_machine $item
}

proc menu_create_load_machine_list {{mode "replace"}} {
	if {$mode == "replace"} {
		set proc_to_exec osd_menu::menu_load_machine_exec_replace
	} elseif {$mode == "add"} {
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

	set items [openmsx_info machines]

	foreach i $items {
		lappend presentation [utils::get_machine_display_name_by_config_name ${i}]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_load_machine_exec_replace { item } {
	if { [catch "machine $item" errorText] } {
		display_osd_text $errorText error
	} else {
		menu_close_all
	}
}

proc menu_load_machine_exec_add { item } {
	set id [create_machine]
	set err [catch { ${id}::load_machine $item } error_result ]
	if {$err} {
		delete_machine $id
		display_osd_text "Error starting [utils::get_machine_display_name_by_config_name $item]: $error_result" error
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

	set items [openmsx_info extensions]
	set presentation [list]

	foreach i $items {
		lappend presentation [utils::get_extension_display_name_by_config_name $i]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_add_extension_exec { item } {
	if { [catch "ext $item" errorText] } {
		display_osd_text $errorText error
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
	set possible_items [openmsx_info extensions]

	set useful_items [list]
	foreach item $items {
		if {[lsearch $possible_items $item] != -1} {
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

proc menu_remove_extension_exec { item } {
	menu_close_all
	remove_extension $item
}

proc menu_create_toys_list {} {
	set menu_def {
	         execute menu_toys_exec
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         header { text "Toys"
	                  font-size 10
	                  post-spacing 6 }}

	set items [info commands toggle_*]

	set presentation [list]
	foreach i $items {
		lappend presentation [string range $i 7 end]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 5 $menu_def]
}

proc menu_toys_exec { toy } {
	return [$toy]
}

proc ls { directory extensions } {
	set roms [glob -nocomplain -tails -directory $directory -type f *.{$extensions}]
	set dirs [glob -nocomplain -tails -directory $directory -type d *]
	set dirs2 [list]
	foreach dir $dirs {
		lappend dirs2 "$dir/"
	}
	return [concat ".." [lsort $dirs2] [lsort $roms]]
}

proc display_osd_text { message {category info} } {
	variable default_bg_color
	variable error_color
	variable warning_color
	switch -- $category {
		"info"    { set bg_color $default_bg_color }
		"warning" { set bg_color $warning_color    }
		"error"   { set bg_color $error_color      }
		"default" { error "Invalid category: $category" }
	}
	
	osd_widgets::text_box display_osd_text \
					-text $message \
					-textrgba 0xffffffff \
					-textsize 6 \
					-rgba $bg_color \
					-x 3 -y 12 -z 5 -w 314 \
					-bordersize 0.5 \
					-borderrgba 0x000000ff \
					-clip true \
					-scaled true \
					-fadeCurrent 1 \
					-fadeTarget 0 \
					-fadePeriod 5
}

proc menu_create_ROM_list { path } {
	return [prepare_menu_list [concat "--eject--" [ls $path "rom,zip,gz"]] \
	                          10 \
	                          { execute menu_select_rom
	                            font-size 8
	                            border-size 2
	                            width 200
	                            xpos 100
	                            ypos 120
	                            header { text "ROMS  $::osd_rom_path"
	                                     font-size 10
                                             post-spacing 6 }}]
}

proc menu_select_rom { item } {
	if [string equal $item "--eject--"] {
		menu_close_all
		carta eject
		reset
	} else {
		set fullname [file join $::osd_rom_path $item]
		if [file isdirectory $fullname] {
			menu_close_top
			set ::osd_rom_path [file normalize $fullname]
			menu_create [menu_create_ROM_list $::osd_rom_path]
		} else {
			menu_close_all
			carta $fullname
			display_osd_text "Now running ROM:\n[rom_info]"
			reset
		}
	}
}

proc menu_create_disk_list { path } {
	return [prepare_menu_list [concat "--eject--" [ls $path "dsk,zip,gz,xsa"]] \
	                          10 \
	                          { execute menu_select_disk
	                            font-size 8
	                            border-size 2
	                            width 200
	                            xpos 100
	                            ypos 120
	                            header { text "Disks  $::osd_disk_path"
	                                     font-size 10
	                                     post-spacing 6 }}]
}

proc menu_select_disk { item } {
	if [string equal $item "--eject--"] {
		menu_close_all
		diska eject
	} else {
		set fullname [file join $::osd_disk_path $item]
		if [file isdirectory $fullname] {
			menu_close_top
			set ::osd_disk_path [file normalize $fullname]
			menu_create [menu_create_disk_list $::osd_disk_path]
		} else {
			menu_close_all
			diska $fullname
		}
	}
}

proc get_savestates_list_presentation_sorted {} {
	set presentation [list]
	foreach i [lsort -integer -index 1 -decreasing [savestate::list_savestates_raw]] {
		if {[info commands clock] != ""} {
			set pres_str [format "%s (%s)" [lindex $i 0] [clock format [lindex $i 1] -format "%x - %X" ]]
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

proc menu_loadstate_select { item } {
	set png $::env(OPENMSX_USER_DATA)/../savestates/${item}.png
	catch {osd create rectangle "preview.image" -relx 0.05 -rely 0.05 -w 80 -h 60 -image $png}
}

proc menu_loadstate_deselect { item } {
	osd destroy "preview.image"
}

proc menu_loadstate_exec { item } {
	menu_close_all
	loadstate $item
}

proc menu_savestate_exec { item } {
	if {$item == "create new"} {
		set item [menu_free_savestate_name]
		menu_close_all
		savestate $item
	} else {
		#TODO "Overwrite are you sure?" -dialog
		menu_close_all
		savestate $item
	}
}

proc menu_free_savestate_name {} {
	set existing [list_savestates]
	set i 1
	while 1 {
		set name [format "savestate%04d" $i]
		if {[lsearch $existing $name] == -1} {
			return $name
		}
		incr i
	}
}

# keybindings
if {$tcl_platform(os) == "Darwin"} { ;# Mac
	bind_default "keyb META+O" main_menu_toggle
} elseif {$is_dingoo} { ;# Dingoo
	bind_default "keyb ESCAPE" main_menu_toggle ;# select button
	bind_default "keyb MENU"   main_menu_toggle ;# default: power+select
} else { ;# any other
	bind_default "keyb MENU"   main_menu_toggle
}

namespace export main_menu_open
namespace export main_menu_close
namespace export main_menu_toggle

} ;# namespace osd_menu

namespace import osd_menu::*
