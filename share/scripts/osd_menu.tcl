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

set menuinfos [list]

proc pack_menu_info {} {
	uplevel {list $name $menutexts $selectinfo $selectidx $scrollidx $on_close}
}
proc unpack_menu_info { data } {
	set cmd [list foreach {name menutexts selectinfo selectidx scrollidx on_close} $data {}]
	uplevel $cmd
}
proc set_selectidx { value } {
	global menuinfos
	lset menuinfos {end 3} $value
}
proc set_scrollidx { value } {
	global menuinfos
	lset menuinfos {end 4} $value
}

proc menu_create { menu_def_list } {
	global menuinfos

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
			set allactions [join [list $defactions $actions]]
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
	global menuinfos
	menu_refresh_helper [lindex $menuinfos end]
}

proc menu_refresh_all {} {
	global menuinfos
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
	global menuinfos
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
	global menuinfos
	while {[llength $menuinfos]} {
		menu_close_top
	}
}

proc menu_updown { delta } {
	global menuinfos
	unpack_menu_info [lindex $menuinfos end]
	menu_on_deselect $selectinfo $selectidx
	set selectidx [expr ($selectidx + $delta) % [llength $selectinfo]]
	set_selectidx $selectidx
	menu_on_select $selectinfo $selectidx
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
proc menu_up {}   { menu_updown -1 }
proc menu_down {} { menu_updown  1 }
proc menu_action { button } {
	global menuinfos
	unpack_menu_info [lindex $menuinfos end]
	array set actions [lindex $selectinfo $selectidx 2]
	set cmd [get_optional actions $button ""]
	uplevel #0 $cmd
	menu_refresh_all
}

user_setting create string osd_rom_path "OSD Rom Load Menu Last Known Path" $env(OPENMSX_USER_DATA)
user_setting create string osd_disk_path "OSD Disk Load Menu Last Known Path" $env(OPENMSX_USER_DATA)
if ![file exists $osd_rom_path] {
	# revert to default (should always exist)
	unset osd_rom_path
}
if ![file exists $osd_disk_path] {
	# revert to default (should always exist)
	unset osd_disk_path
}

proc main_menu_open {} {
	menu_create $::main_menu

	set ::pause true
	# TODO make these bindings easier to customize
	bind_default "keyb UP"     -repeat { menu_action UP    }
	bind_default "keyb DOWN"   -repeat { menu_action DOWN  }
	bind_default "keyb LEFT"   -repeat { menu_action LEFT  }
	bind_default "keyb RIGHT"  -repeat { menu_action RIGHT }
	bind_default "keyb SPACE"          { menu_action A     }
	bind_default "keyb RETURN"         { menu_action A     }
	bind_default "keyb ESCAPE"         { menu_action B     }
}
proc main_menu_close {} {
	menu_close_all
}
proc main_menu_toggle {} {
	global menuinfos
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
	set_optional actions UP   menu_up
	set_optional actions DOWN menu_down
	set_optional actions B    menu_close_top
	set menudef(actions) [array get actions]
	return [array get menudef]
}

proc prepare_menu_list { lst num menu_def_list } {
	array set menudef $menu_def_list
	set execute $menudef(execute)
	set header $menudef(header)
	set item_extra  [get_optional menudef item ""]
	set on_select   [get_optional menudef on-select ""]
	set on_deselect [get_optional menudef on-deselect ""]
	lappend header "selectable" "false"
	set items [list $header]
	for {set i 0} {$i < $num} {incr i} {
		set actions [list "A" "list_menu_item_exec $execute \{$lst\} $i"]
		if {$i == 0} {
			lappend actions "UP" "list_menu_item_up"
		}
		if {$i == ($num - 1)} {
			lappend actions "DOWN" "list_menu_item_down [llength $lst] $i"
		}
		set item [list "text" "\[list_menu_item_show \{$lst\} $i\]" \
		               "actions" $actions]
		if {$on_select != ""} {
			lappend item "on-select" "list_menu_item_select \{$lst\} $i $on_select"
		}
		if {$on_deselect != ""} {
			lappend item "on-deselect" "list_menu_item_select \{$lst\} $i $on_deselect"
		}
		lappend items [join [list $item $item_extra]]
	}
	set menudef(items) $items
	return [prepare_menu [array get menudef]]
}
proc list_menu_item_exec { execute lst pos } {
	global menuinfos
	unpack_menu_info [lindex $menuinfos end]
	$execute [lindex $lst [expr $pos + $scrollidx]]
}
proc list_menu_item_show { lst pos } {
	global menuinfos
	unpack_menu_info [lindex $menuinfos end]
	return [lindex $lst [expr $pos + $scrollidx]]
}
proc list_menu_item_select { lst pos select_proc } {
	global menuinfos
	unpack_menu_info [lindex $menuinfos end]
	$select_proc [lindex $lst [expr $pos + $scrollidx]]
}
proc list_menu_item_up { } {
	global menuinfos
	unpack_menu_info [lindex $menuinfos end]
	if {$scrollidx > 0} {incr scrollidx -1}
	set_scrollidx $scrollidx
	menu_refresh_top
}
proc list_menu_item_down { size pos } {
	global menuinfos
	unpack_menu_info [lindex $menuinfos end]
	if {($scrollidx + $pos + 1) < $size} {incr scrollidx}
	set_scrollidx $scrollidx
	menu_refresh_top
}

set main_menu [prepare_menu {
	bg-color 0x000000a0
	text-color 0xffffffff
	select-color 0x8080ffd0
	font-size 12
	border-size 2
	width 160
	items {{ text "openMSX Menu"
	         text-color 0x00ffffff
	         font-size 20
	         post-spacing 6
	         selectable false }
	       { text "Load ROM..."
	         actions { A { menu_create [create_ROM_list $::osd_rom_path] }}}
	       { text "Insert Disk..."
	         actions { A { menu_create [create_disk_list $::osd_disk_path] }}
	         post-spacing 3 }
	       { text "Save state..."
	         actions { A { menu_create [create_save_state] }}}
	       { text "Load state..."
	         actions { A { menu_create [create_load_state] }}
	         post-spacing 3 }
	       { text "Settings..."
	         actions { A { menu_create $::setting_menu }}
	         post-spacing 3 }
	       { text "Reset MSX"
	         actions { A { reset ; menu_close_all }}}
	       { text "Exit openMSX"
	         actions { A exit }}}}]

set setting_menu [prepare_menu {
	bg-color 0x000000a0
	text-color 0xffffffff
	select-color 0x8080ffd0
	font-size 12
	border-size 2
	width 150
	xpos 100
	ypos 120
	items {{ text "Settings"
	         text-color 0xffff40ff
	         font-size 20
	         post-spacing 6
	         selectable false }
	       { text "speed: $speed"
	         actions { LEFT  { incr speed -5 }
	                   RIGHT { incr speed  5 }}}
	       { text "volume: $master_volume"
	         actions { LEFT  { incr master_volume -5 }
	                   RIGHT { incr master_volume  5 }}}
	       { text "scaler: $scale_algorithm"
	         actions { LEFT  { cycle_back scale_algorithm }
	                   RIGHT { cycle scale_algorithm }}}}}]


proc __ls { directory } {
	set roms [glob -nocomplain -tails -directory $directory -type f *.{rom,zip,gz}]
	set dirs [glob -nocomplain -tails -directory $directory -type d *]
	set dirs2 [list]
	foreach dir $dirs {
		lappend dirs2 "$dir/"
	}
	return [join [list ".." [lsort $dirs2] [lsort $roms]]]
}

proc __displayOSDText { message } {
	if ![info exists ::__displayOSDText_bg] {
		set ::__displayOSDText_bg  [osd create rectangle "displayOSDText" \
		                                -x 3 -y 12 -z 5 -w 314 -h 9 \
		                                -rgb 0x002090 -scaled true -clip true]
		set ::__displayOSDText_txt [osd create text "displayOSDText.txt" \
		                                -size 6 -rgb 0xffffff \
		                                -font "skins/Vera.ttf.gz"]
	}
	osd configure $::__displayOSDText_bg  -alpha 190 \
	                                      -fadeTarget 0 -fadePeriod 5.0
	osd configure $::__displayOSDText_txt -alpha 255 -text $message \
	                                      -fadeTarget 0 -fadePeriod 5.0
}

proc create_ROM_list { path } {
	return [prepare_menu_list [__ls $path] \
	                          10 \
	                          { execute __menu_select_rom
	                            bg-color 0x000000a0
	                            text-color 0xffffffff
	                            select-color 0x8080ffd0
	                            font-size 6
	                            border-size 2
	                            width 200
	                            xpos 100
	                            ypos 120
	                            header { text "ROMS  $::osd_rom_path"
	                                     text-color 0xff0000ff
	                                     font-size 10 }}]
}

proc __menu_select_rom { item } {
	set fullname [file join $::osd_rom_path $item]
	if [file isdirectory $fullname] {
		menu_close_top
		set ::osd_rom_path [file normalize $fullname]
		menu_create [create_ROM_list $::osd_rom_path]
	} else {
		menu_close_all
		carta $fullname
		__displayOSDText "Now running ROM: $item"
		reset
	}
}

proc create_disk_list { path } {
	set disks [concat "--eject--" [__ls $path]]
	return [prepare_menu_list $disks \
	                          10 \
	                          { execute __menu_select_disk
	                            bg-color 0x000000a0
	                            text-color 0xffffffff
	                            select-color 0x8080ffd0
	                            font-size 6
	                            border-size 2
	                            width 200
	                            xpos 100
	                            ypos 120
	                            header { text "Disks  $::osd_disk_path"
	                                     text-color 0xff0000ff
	                                     font-size 10 }}]
}

proc __menu_select_disk { item } {
	if [string equal $item "--eject--"] {
		menu_close_all
		diska eject
	} else {
		set fullname [file join $::osd_disk_path $item]
		if [file isdirectory $fullname] {
			menu_close_top
			set ::osd_disk_path [file normalize $fullname]
			menu_create [create_disk_list $::osd_disk_path]
		} else {
			menu_close_all
			diska $fullname
		}
	}
}

proc create_load_state {} {
	return [prepare_menu_list [list_savestates] 10 \
	       { execute menu_loadstate_exec
	         bg-color 0x000000a0
	         text-color 0xffffffff
	         select-color 0x8080ffd0
	         font-size 6
	         border-size 2
	         width 100
	         xpos 100
	         ypos 120
	         on-open  {osd create rectangle "preview" -x 225 -y 5 -w 90 -h 70 -rgba 0x30303080 -scaled true}
	         on-close {osd destroy "preview"}
	         on-select   menu_loadstate_select
	         on-deselect menu_loadstate_deselect
	         header { text "loadstate"
	                  text-color 0x00ff00ff
	                  font-size 10}}]
}
proc create_save_state {} {
	set items [concat [list "create new"] [list_savestates]]
	return [prepare_menu_list $items 10 \
	       { execute menu_savestate_exec
	         bg-color 0x000000a0
	         text-color 0xffffffff
	         select-color 0x8080ffd0
	         font-size 6
	         border-size 2
	         width 100
	         xpos 100
	         ypos 120
	         on-open  {osd create rectangle "preview" -x 225 -y 5 -w 90 -h 70 -rgba 0x30303080 -scaled true}
	         on-close {osd destroy "preview"}
	         on-select   menu_loadstate_select
	         on-deselect menu_loadstate_deselect
	         header { text "savestate"
	                  text-color 0xff0000ff
	                  font-size 10}}]
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

bind_default "keyb MENU" main_menu_toggle

