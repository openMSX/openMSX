namespace eval osd_menu {

set_help_text main_menu_open   "(experimental) Show the OSD menu."
set_help_text main_menu_close  "(experimental) Remove the OSD menu."
set_help_text main_menu_toggle "(experimental) Toggle the OSD menu."


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

variable menuinfos [list]
variable main_menu

proc pack_menu_info {} {
	uplevel {list $name $menutexts $selectinfo $selectidx $scrollidx $on_close}
}
proc unpack_menu_info { data } {
	set cmd [list foreach {name menutexts selectinfo selectidx scrollidx on_close} $data {}]
	uplevel $cmd
}
proc set_selectidx { value } {
	variable menuinfos
	lset menuinfos {end 3} $value
}
proc set_scrollidx { value } {
	variable menuinfos
	lset menuinfos {end 4} $value
}

proc menu_create { menu_def_list } {
	variable menuinfos

	set name "menu[expr [llength $menuinfos] + 1]"

	array set menudef $menu_def_list

	set defactions   [get_optional menudef "actions" ""]
	set bgcolor      [get_optional menudef "bg-color" 0]
	set deftextcolor [get_optional menudef "text-color" 0xffffffff]
	set selectcolor  [get_optional menudef "select-color" 0x0000ffff]
	set deffontsize  [get_optional menudef "font-size" 12]
	set deffont      [get_optional menudef "font" "skins/Vera.ttf.gz"]
	set bordersize   [get_optional menudef "border-size" 0]
	set on_open      [get_optional menudef "on-open" ""]
	set on_close     [get_optional menudef "on-close" ""]

	osd create rectangle $name -scaled true -rgba $bgcolor -clip true
	set y $bordersize
	set selectinfo [list]
	set menutexts [list]
	set items $menudef(items)
	foreach itemdef $items {
		array unset itemarr
		array set itemarr $itemdef
		incr y [get_optional itemarr "pre-spacing" 0]
		set fontsize  [get_optional itemarr "font-size"  $deffontsize]
		set font      [get_optional itemarr "font"       $deffont]
		set textcolor [get_optional itemarr "text-color" $deftextcolor]
		set actions   [get_optional itemarr "actions"    ""]
		set on_select   [get_optional itemarr "on-select"   ""]
		set on_deselect [get_optional itemarr "on-deselect" ""]
		set textid "${name}.item${y}"
		set text $itemarr(text)
		lappend menutexts $textid $text
		osd create text $textid -font $font -size $fontsize \
					-rgba $textcolor -x $bordersize -y $y
		set selectable [get_optional itemarr "selectable" true]
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

	set selectidx 0
	set scrollidx 0
	lappend menuinfos [pack_menu_info]

	uplevel #0 $on_open
	menu_on_select $selectinfo $selectidx

	menu_refresh_top
}

proc menu_refresh_top {} {
	variable menuinfos
	menu_refresh_helper [lindex $menuinfos end]
}

proc menu_refresh_all {} {
	variable menuinfos
	foreach menuinfo $menuinfos {
		menu_refresh_helper $menuinfo
	}
}

proc menu_refresh_helper { menuinfo } {
	unpack_menu_info $menuinfo

	foreach { osdid text } $menutexts {
		set cmd [list subst $text]
		osd configure $osdid -text [uplevel #0 $cmd]
	}

	set sely [lindex $selectinfo $selectidx 0]
	set selh [lindex $selectinfo $selectidx 1]
	osd configure "${name}.selection" -y $sely -h $selh
}

proc menu_close_top {} {
	variable menuinfos
	unpack_menu_info [lindex $menuinfos end]
	menu_on_deselect $selectinfo $selectidx
	uplevel #0 $on_close
	osd destroy $name
	set menuinfos [lreplace $menuinfos end end]
	if {[llength $menuinfos] == 0} {
		menu_last_closed
	}
}

proc menu_close_all {} {
	variable menuinfos
	while {[llength $menuinfos]} {
		menu_close_top
	}
}

proc menu_updown { delta } {
	variable menuinfos
	unpack_menu_info [lindex $menuinfos end]
	menu_move_selection $selectinfo \
		$selectidx [expr ($selectidx + $delta) % [llength $selectinfo]]
	menu_refresh_top
}
proc menu_move_selection { selectinfo from_idx to_idx } {
	menu_on_deselect $selectinfo $from_idx
	set_selectidx $to_idx
	menu_on_select $selectinfo $to_idx
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
	variable menuinfos
	unpack_menu_info [lindex $menuinfos end]
	array set actions [lindex $selectinfo $selectidx 2]
	set cmd [get_optional actions $button ""]
	uplevel #0 $cmd
}

user_setting create string osd_rom_path "OSD Rom Load Menu Last Known Path" $env(OPENMSX_USER_DATA)
user_setting create string osd_disk_path "OSD Disk Load Menu Last Known Path" $env(OPENMSX_USER_DATA)
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

	# close console, because the menu interferes with it
	set ::console off

	menu_create $main_menu

	set ::pause true
	# TODO make these bindings easier to customize
	bind_default "keyb UP"     -repeat { osd_menu::menu_action UP    }
	bind_default "keyb DOWN"   -repeat { osd_menu::menu_action DOWN  }
	bind_default "keyb LEFT"   -repeat { osd_menu::menu_action LEFT  }
	bind_default "keyb RIGHT"  -repeat { osd_menu::menu_action RIGHT }
	bind_default "keyb SPACE"          { osd_menu::menu_action A     }
	bind_default "keyb RETURN"         { osd_menu::menu_action A     }
	bind_default "keyb ESCAPE"         { osd_menu::menu_action B     }
}
proc main_menu_close {} {
	menu_close_all
}
proc main_menu_toggle {} {
	variable menuinfos
	if [llength $menuinfos] {
		# there is at least one menu open, close it
		menu_close_all
	} else {
		# none open yet, open main menu
		main_menu_open
	}
}

proc menu_last_closed {} {
	set ::pause false
	# TODO avoid duplication with 'main_menu_open'
	unbind_default "keyb UP"
	unbind_default "keyb DOWN"
	unbind_default "keyb LEFT"
	unbind_default "keyb RIGHT"
	unbind_default "keyb SPACE"
	unbind_default "keyb RETURN"
	unbind_default "keyb ESCAPE"
}

proc prepare_menu { menu_def_list } {
	array set menudef $menu_def_list
	array set actions [get_optional menudef actions ""]
	set_optional actions UP   { osd_menu::menu_updown -1 }
	set_optional actions DOWN { osd_menu::menu_updown  1 }
	set_optional actions B    { osd_menu::menu_close_top }
	set menudef(actions) [array get actions]
	return [array get menudef]
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
	lappend header "selectable" "false"
	set items [list $header]
	set lst_len [llength $lst]
	set menu_len [expr $lst_len < $num ? $lst_len : $num]
	for {set i 0} {$i < $menu_len} {incr i} {
		set actions [list "A" "osd_menu::list_menu_item_exec $execute \{$lst\} $i"]
		if {$i == 0} {
			lappend actions "UP" "osd_menu::list_menu_item_updown -1 $lst_len $menu_len"
		}
		if {$i == ($menu_len - 1)} {
			lappend actions "DOWN" "osd_menu::list_menu_item_updown 1 $lst_len $menu_len"
		}
		set item [list "text" "\[osd_menu::list_menu_item_show \{$presentation\} $i\]" \
		               "actions" $actions]
		if {$on_select != ""} {
			lappend item "on-select" "osd_menu::list_menu_item_select \{$lst\} $i $on_select"
		}
		if {$on_deselect != ""} {
			lappend item "on-deselect" "osd_menu::list_menu_item_select \{$lst\} $i $on_deselect"
		}
		lappend items [concat $item $item_extra]
	}
	set menudef(items) $items
	return [prepare_menu [array get menudef]]
}
proc list_menu_item_exec { execute lst pos } {
	variable menuinfos
	unpack_menu_info [lindex $menuinfos end]
	$execute [lindex $lst [expr $pos + $scrollidx]]
}
proc list_menu_item_show { lst pos } {
	variable menuinfos
	unpack_menu_info [lindex $menuinfos end]
	return [lindex $lst [expr $pos + $scrollidx]]
}
proc list_menu_item_select { lst pos select_proc } {
	variable menuinfos
	unpack_menu_info [lindex $menuinfos end]
	$select_proc [lindex $lst [expr $pos + $scrollidx]]
}
proc list_menu_item_updown { delta listsize menusize } {
	variable menuinfos
	unpack_menu_info [lindex $menuinfos end]
	incr scrollidx $delta
	set itemidx [expr $scrollidx + $selectidx]
	if {$itemidx < 0} {
		set_scrollidx [expr $listsize - $menusize]
		menu_move_selection $selectinfo $selectidx [expr $menusize - 1]
	} elseif {$itemidx >= $listsize} {
		set_scrollidx 0
		menu_move_selection $selectinfo $selectidx 0
	} else {
		menu_on_deselect $selectinfo $selectidx
		set_scrollidx $scrollidx
		menu_on_select $selectinfo $selectidx
	}
	menu_refresh_top
}

#
# definitions of menus
#

set main_menu [prepare_menu {
	bg-color 0x000000a0
	text-color 0xffffffff
	select-color 0x8080ffd0
	font-size 10
	border-size 2
	width 160
	items {{ text "Menu"
	         text-color 0x00ffffff
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
	       { text "openMSX Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::misc_setting_menu }}}
	       { text "Sound Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::sound_setting_menu }}}
	       { text "Video Settings..."
	         actions { A { osd_menu::menu_create $osd_menu::video_setting_menu }}
	         post-spacing 3 }
	       { text "Manage Running Machines..."
	         actions { A { osd_menu::menu_create $osd_menu::running_machines_menu }}
	         post-spacing 10 }
	       { text "Reset MSX"
	         actions { A { reset ; osd_menu::menu_close_all }}}
	       { text "Exit openMSX"
	         actions { A exit }}}}]

set misc_setting_menu [prepare_menu {
	bg-color 0x000000a0
	text-color 0xffffffff
	select-color 0x8080ffd0
	font-size 8
	border-size 2
	width 150
	xpos 100
	ypos 120
	items {{ text "Misc Settings"
	         text-color 0xffff40ff
	         font-size 12
	         post-spacing 6
	         selectable false }
	       { text "Speed: $speed"
	         actions { LEFT  { incr speed -1 }
	                   RIGHT { incr speed  1 }}}
	       { text "Minimal Frameskip: $minframeskip"
	         actions { LEFT  { incr minframeskip -1 }
	                   RIGHT { incr minframeskip  1 }}}
	       { text "Maximal Frameskip: $maxframeskip"
	         actions { LEFT  { incr maxframeskip -1 }
	                   RIGHT { incr maxframeskip  1 }}}}}]

set sound_setting_menu [prepare_menu {
	bg-color 0x000000a0
	text-color 0xffffffff
	select-color 0x8080ffd0
	font-size 8
	border-size 2
	width 150
	xpos 100
	ypos 120
	items {{ text "Sound Settings"
	         text-color 0xffff40ff
	         font-size 12
	         post-spacing 6
	         selectable false }
	       { text "Volume: $master_volume"
	         actions { LEFT  { incr master_volume -5 }
	                   RIGHT { incr master_volume  5 }}}
	       { text "Mute: $mute"
	         actions { LEFT  { cycle_back mute }
	                   RIGHT { cycle mute }}}}}]

set video_setting_menu [prepare_menu {
	bg-color 0x000000a0
	text-color 0xffffffff
	select-color 0x8080ffd0
	font-size 8
	border-size 2
	width 150
	xpos 100
	ypos 120
	items {{ text "Video Settings"
	         text-color 0xffff40ff
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Scaler: $scale_algorithm"
	         actions { LEFT  { cycle_back scale_algorithm }
	                   RIGHT { cycle scale_algorithm }}}
	       { text "Scale Factor: $scale_factor X"
	         actions { LEFT  { incr scale_factor -1 }
	                   RIGHT { incr scale_factor  1 }}
	         post-spacing 6 }
	       { text "Scanline: $scanline"
	         actions { LEFT  { incr scanline -1 }
	                   RIGHT { incr scanline  1 }}}
	       { text "Blur: $blur"
	         actions { LEFT  { incr blur -1 }
	                   RIGHT { incr blur  1 }}}
	       { text "Glow: $glow"
	         actions { LEFT  { incr glow -1 }
	                   RIGHT { incr glow  1 }}}}}]

set running_machines_menu [prepare_menu {
	bg-color 0x000000a0
	text-color 0xffffffff
	select-color 0x8080ffd0
	font-size 8
	border-size 2
	width 175
	xpos 100
	ypos 120
	items {{ text "Manage Running Machines"
	         text-color 0xffff40ff
	         font-size 10
	         post-spacing 6
	         selectable false }
	       { text "Select Running Machine Tab: [utils::get_machine_display_name]"
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_running_machine_list] }}}
	       { text "New Running Machine Tab"
	         actions { A { osd_menu::menu_create [osd_menu::menu_create_load_machine_list] }}}
	       { text "Close Current Machine Tab"
	         actions { A { set old_active_machine [activate_machine]; cycle_machine; delete_machine $old_active_machine }}}}}]

proc menu_create_running_machine_list {} {
	set menu_def {
	         execute menu_machine_tab_select_exec
	         bg-color 0x000000a0
	         text-color 0xffffffff
	         select-color 0x8080ffd0
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         header { text "Select Running Machine"
	                  text-color 0xff0000ff
	                  font-size 12
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

proc menu_create_load_machine_list {} {
	set menu_def {
	         execute osd_menu::menu_load_machine_exec
	         bg-color 0x000000a0
	         text-color 0xffffffff
	         select-color 0x8080ffd0
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         header { text "Select Machine to Run"
	                  text-color 0xff0000ff
	                  font-size 12
	                  post-spacing 6 }}

	set items [openmsx_info machines]

	foreach i $items {
		lappend presentation [utils::get_machine_display_name_by_config_name ${i}]
	}
	lappend menu_def presentation $presentation

	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_load_machine_exec { item } {
	menu_close_top
	set id [create_machine]
	set err [catch { ${id}::load_machine $item } error_result ]
	if {$err} {
		delete_machine $id
		puts "Error starting [utils::get_machine_display_name_by_config_name $item]: $error_result" ;# how/where to log this better?
	} else {
		activate_machine $id
	}
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

variable display_osd_text_created false

proc display_osd_text { message } {
	variable display_osd_text_created
	if !$display_osd_text_created {
		osd create rectangle "display_osd_text" \
		                                -x 3 -y 12 -z 5 -w 314 -h 9 \
		                                -rgb 0x002090 -scaled true -clip true
		osd create text "display_osd_text.txt" \
		                                -size 6 -rgb 0xffffff
		set display_osd_text_created true
	}
	osd configure display_osd_text  -alpha 190 \
	                                      -fadeTarget 0 -fadePeriod 5.0
	osd configure display_osd_text.txt -alpha 255 -text $message \
	                                      -fadeTarget 0 -fadePeriod 5.0
}

proc menu_create_ROM_list { path } {
	return [prepare_menu_list [ls $path "rom,zip,gz"] \
	                          10 \
	                          { execute menu_select_rom
	                            bg-color 0x000000a0
	                            text-color 0xffffffff
	                            select-color 0x8080ffd0
	                            font-size 8
	                            border-size 2
	                            width 200
	                            xpos 100
	                            ypos 120
	                            header { text "ROMS  $::osd_rom_path"
	                                     text-color 0xff0000ff
	                                     font-size 10
                                             post-spacing 6 }}]
}

proc menu_select_rom { item } {
	set fullname [file join $::osd_rom_path $item]
	if [file isdirectory $fullname] {
		menu_close_top
		set ::osd_rom_path [file normalize $fullname]
		menu_create [menu_create_ROM_list $::osd_rom_path]
	} else {
		menu_close_all
		carta $fullname
		display_osd_text "Now running ROM: $item"
		reset
	}
}

proc menu_create_disk_list { path } {
	set disks [concat "--eject--" [ls $path "dsk,zip,gz"]]
	return [prepare_menu_list $disks \
	                          10 \
	                          { execute menu_select_disk
	                            bg-color 0x000000a0
	                            text-color 0xffffffff
	                            select-color 0x8080ffd0
	                            font-size 8
	                            border-size 2
	                            width 200
	                            xpos 100
	                            ypos 120
	                            header { text "Disks  $::osd_disk_path"
	                                     text-color 0xff0000ff
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
	foreach i [lsort -integer -index 1 [savestate::list_savestates_raw]] {
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
	         bg-color 0x000000a0
	         text-color 0xffffffff
	         select-color 0x8080ffd0
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         on-open  {osd create rectangle "preview" -x 225 -y 5 -w 90 -h 70 -rgba 0x30303080 -scaled true}
	         on-close {osd destroy "preview"}
	         on-select   menu_loadstate_select
	         on-deselect menu_loadstate_deselect
	         header { text "Loadstate"
	                  text-color 0x00ff00ff
	                  font-size 12
                          post-spacing 6 }}

	set items [list_savestates -t]

	lappend menu_def presentation [get_savestates_list_presentation_sorted]

	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_create_save_state {} {
	set items [concat [list "create new"] [list_savestates -t]]
	set menu_def \
	       { execute menu_savestate_exec
	         bg-color 0x000000a0
	         text-color 0xffffffff
	         select-color 0x8080ffd0
	         font-size 8
	         border-size 2
	         width 200
	         xpos 100
	         ypos 120
	         on-open  {osd create rectangle "preview" -x 225 -y 5 -w 90 -h 70 -rgba 0x30303080 -scaled true}
	         on-close {osd destroy "preview"}
	         on-select   menu_loadstate_select
	         on-deselect menu_loadstate_deselect
	         header { text "Savestate"
	                  text-color 0xff0000ff
	                  font-size 12
	                  post-spacing 6 }}


	lappend menu_def presentation [concat [list "create new"] [get_savestates_list_presentation_sorted]]

	return [prepare_menu_list $items 10 $menu_def]
}

proc menu_loadstate_select { item } {
	set png $::env(OPENMSX_USER_DATA)/../savestates/${item}.png
	catch {osd create rectangle "preview.image" -relx 0.05 -rely 0.05 -w 80 -h 60 -image $png}
}
proc menu_loadstate_deselect { item } {
	catch {osd destroy "preview.image"}
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
if {$tcl_platform(os) == "Darwin"} {
	bind_default "keyb META+O" main_menu_toggle
} else {
	bind_default "keyb MENU" main_menu_toggle
}

namespace export main_menu_open
namespace export main_menu_close
namespace export main_menu_toggle

} ;# namespace osd_menu

namespace import osd_menu::*
