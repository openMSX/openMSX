set menuevent "keyb MENU"

proc get_optional { array_name key default } {
	upvar $array_name arr
	if [info exists arr($key)] {
		return $arr($key)
	} else {
		return $default
	}
}

set menuinfos [list]

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

	osd create rectangle $name -scaled true -rgba $bgcolor
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
		set textid "${name}.item${y}"
		set text $itemarr(text)
		lappend menutexts $textid $text
		osd create text $textid -font $font -size $fontsize \
		                        -rgba $textcolor -x $bordersize -y $y
		set selectable [get_optional itemarr "selectable" true]
		if $selectable {
			set allactions [join [list $defactions $actions]]
			lappend selectinfo [list $y $fontsize $allactions]
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
	set menuinfo [list $name $menutexts $selectinfo $selectidx]
	lappend menuinfos $menuinfo

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
	foreach {name menutexts selectinfo selectidx} $menuinfo {}

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
	foreach {name menutexts selectinfo selectidx} [lindex $menuinfos end] {}
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

proc menu_up {} {
	global menuinfos
	foreach {name menutexts selectinfo selectidx} [lindex $menuinfos end] {}
	set newidx [expr ($selectidx - 1) % [llength $selectinfo]]
	lset menuinfos {end 3} $newidx
	menu_refresh_top
}
proc menu_down {} {
	global menuinfos
	foreach {name menutexts selectinfo selectidx} [lindex $menuinfos end] {}
	set newidx [expr ($selectidx + 1) % [llength $selectinfo]]
	lset menuinfos {end 3} $newidx
	menu_refresh_top
}
proc menu_left {} {
	menu_action LEFT
}
proc menu_right {} {
	menu_action RIGHT
}
proc menu_a {} {
	menu_action A
}
proc menu_b {} {
	menu_action B
}
proc menu_action { button } {
	global menuinfos
	foreach {name menutexts selectinfo selectidx} [lindex $menuinfos end] {}
	array set actions [lindex $selectinfo $selectidx 2]
	set cmd [get_optional actions $button ""]
	uplevel #0 $cmd
	menu_refresh_all
}


proc main_menu_open {} {
	menu_create $::main_menu

	set ::pause true
	bind_default "keyb UP"     menu_up
	bind_default "keyb DOWN"   menu_down
	bind_default "keyb LEFT"   menu_left
	bind_default "keyb RIGHT"  menu_right
	bind_default "keyb SPACE"  menu_a
	bind_default "keyb ESCAPE" menu_b
	bind_default $::menuevent  main_menu_close
}
proc main_menu_close {} {
	menu_close_all
}
proc menu_last_closed {} {
	set ::pause false
	unbind_default "keyb UP"
	unbind_default "keyb DOWN"
	unbind_default "keyb LEFT"
	unbind_default "keyb RIGHT"
	unbind_default "keyb SPACE"
	unbind_default "keyb ESCAPE"
        bind_default   $::menuevent main_menu_open
}

set main_menu { actions { B menu_close_top }
                bg-color 0x00000080
                text-color 0xffffffff
                select-color 0x8080ffc0
                font-size 12
                border-size 2
                width 150
                items {{ text "My Cool Menu"
                         text-color 0x00ffffff
                         font-size 20
                         post-spacing 6
                         selectable false }
                       { text "menu item 1"
                         actions { A execute-A }
                         post-spacing 3 }
                       { text "settings..."
                         actions { A { menu_create $::setting_menu }}}
                       { pre-spacing 6
                         text "sub title"
                         text-color 0xc0ffffff
                         font-size 15
                         post-spacing 3
                         selectable false }
                       { text "exit"
                         actions { A exit }}}}

set setting_menu { actions { B menu_close_top }
                   bg-color 0x00000080
                   text-color 0xffffffff
                   select-color 0x8080ffc0
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
                          { text "scanline: $scanline"
                            actions { LEFT  { incr scanline -5 }
                                      RIGHT { incr scanline  5 }}}
			  { text "scaler: $scale_algorithm"
                            actions { LEFT  { cycle scale_algorithm }
                                      RIGHT { cycle scale_algorithm }}}}}

bind_default $menuevent main_menu_open

