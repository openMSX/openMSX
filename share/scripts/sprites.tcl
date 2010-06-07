#attributes
#expr ([vdpreg 11] << 15) + (([vdpreg 5] & 0xf8) << 7)

namespace eval test_osd_menu {

#todo: more sprite info / better layout (sprite mode/ hex values/ any other info)

variable title_pos 0
variable max_sprites 0

proc sprite_menu {} {

	bind_default "keyb LEFT"    -repeat { test_osd_menu::sprite_menu_action 1 }
	bind_default "keyb RIGHT"   -repeat { test_osd_menu::sprite_menu_action 2 }
	bind_default "keyb ESCAPE"  -repeat	{ test_osd_menu::sprite_menu_hide	}
	bind_default "keyb SPACE"   -repeat	{ test_osd_menu::sprite_menu_action 0}

	osd create rectangle sprite_menu -x 100 -y 100 -w 255 -h 192 -rgba 0x00000080
	osd create rectangle sprite_menu.title -x 0 -y 15 -w 255 -h 32 -rgba 0x008000ff -clip on
 	osd create text	sprite_menu.title.text -x 24 -text "" -size 24 -rgba 0xffffffff
	osd create text sprite_menu.index -x 184 -y 15 -text "" -size 10 -rgba 0xffffffff
	osd create text sprite_menu.refresh -x 130 -y 24 -size 10 -text "\[Space\] Refresh Sprite" -rgba 0xffffffff
	osd create text sprite_menu.escape -x 130 -y 35 -size 10 -text "\[Escape\] to Exit Viewer" -rgba 0xffffffff
	
	sprite_menu_action 0
}

proc sprite_menu_action {action} {

	variable title
	variable title_pos
	variable max_sprites

	if {$action==1} {incr title_pos -1}
	if {$action==2} {incr title_pos 1}

	if {$title_pos>$max_sprites} {set title_pos 0}
	if {$title_pos<0} {set title_pos $max_sprites}

	#show sprite matrix
	show_sprite $title_pos

	#fade/ease
	osd configure sprite_menu.title.text -text "Sprite $title_pos" -fadeCurrent 0
	osd configure sprite_menu.index	-text "In mem: $max_sprites"
	ease_text sprite_menu.title.text 0 $action

}

proc sprite_menu_hide {} {
	osd destroy sprite_menu
	unbind_default "keyb LEFT"
	unbind_default "keyb RIGHT"
	unbind_default "keyb ESCAPE"
	unbind_default "keyb SPACE"
}

proc ease_text {osd_object {frame_render 0} {action 0}} {

	#Ease in length
	set x 16

	#Intro Fade
	if {$frame_render==0} {
	   osd configure $osd_object -x $x -fadeTarget 1 -fadePeriod 0.25
	}

	if {$frame_render>$x} {
		#leave routine
		return ""
	}

	#ease in
	if {$action==2} {
	osd configure $osd_object -x [expr (($x*-1)+$frame_render)]
	} else {
	osd configure $osd_object -x [expr ($x-$frame_render)]	
	}

	incr frame_render 1

	#call same function
	after realtime 0 [namespace code [list ease_text $osd_object $frame_render $action]]
}

proc show_sprite {sprite} {
	variable max_sprites
	
	set sprite_size [expr ([vdpreg 1] & 2) ? 16 : 8]
	set factor [expr ($sprite_size == 8) ? 1 : 4]
	set max_sprites [expr (256 / $factor) - 1]
	if {($sprite > $max_sprites) || ($sprite < 0)} {
		error "Please choose a value between 0 and $max_sprites"
	}

	catch { osd destroy sprite_menu.sprite_osd }
	draw_matrix "sprite_menu.sprite_osd" 8 56 8 $sprite_size 1

	set addr [expr ([vdpreg 6] << 11) + ($sprite * $factor * 8)]
	for {set y 0} {$y < $sprite_size} {incr y; incr addr} {
		set pattern [vpeek $addr]
		set mask [expr ($sprite_size == 8) ? 0x80 : 0x8000]
		if {$sprite_size == 16} {
			set pattern [expr ($pattern << 8) + [vpeek [expr $addr + 16]]]
		}
		for {set x 0} {$x < $sprite_size} {incr x} {
			if {$pattern & $mask} {
				osd configure sprite_menu.sprite_osd.${x}_${y} -rgba 0xffffffff
			}
			set mask [expr $mask >> 1]
		}
	}
}

proc draw_matrix {matrixname x y blocksize matrixsize matrixgap} {
	osd create rectangle $matrixname \
		-x $x \
		-y $y \
		-h [expr $matrixsize * $blocksize] \
		-w [expr $matrixsize * $blocksize] \
		-rgba 0x00000040

	for {set x 0} {$x < $matrixsize} {incr x} {
		for {set y 0} {$y < $matrixsize} {incr y} {
			osd create rectangle $matrixname.${x}_${y} \
				-h [expr $blocksize - $matrixgap] \
				-w [expr $blocksize - $matrixgap] \
				-x [expr $x * $blocksize] \
				-y [expr $y * $blocksize] \
				-rgba 0x00800080
		}
	}


	return ""
}

namespace export sprite_menu

} ;# namespace osd_menu

namespace import test_osd_menu::*
