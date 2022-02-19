namespace eval osd_sprite_info {

#todo: more sprite info / better layout (sprite mode/ hex values/ any other info)

variable title_pos 0
variable max_sprites 0

proc sprite_viewer {} {
	bind -layer sprite_viewer "keyb LEFT"   -repeat {osd_sprite_info::sprite_viewer_action 1}
	bind -layer sprite_viewer "keyb RIGHT"  -repeat {osd_sprite_info::sprite_viewer_action 2}
	bind -layer sprite_viewer "keyb ESCAPE" -repeat {osd_sprite_info::sprite_viewer_hide}
	bind -layer sprite_viewer "keyb SPACE"  -repeat {osd_sprite_info::sprite_viewer_action 0}
	activate_input_layer sprite_viewer

	osd create rectangle sprite_viewer -x 5 -y 100 -w 128 -h 190 -rgba 0x00000080
	osd create rectangle sprite_viewer.title -x 0 -y 15 -w 128 -h 32 -rgba 0x0000ff80 -clip on
	osd create text sprite_viewer.title.text -x 24 -text "" -size 18 -rgba 0xffffffff
	osd create text sprite_viewer.index -x 70 -y 38 -text "" -size 8 -rgba 0xffffffff
	osd create text sprite_viewer.refresh -x 8 -y 170 -size 8 -text "\[Space\] Refresh Sprite" -rgba 0xffffffff
	osd create text sprite_viewer.escape -x 8 -y 180 -size 8 -text "\[Escape\] to Exit Viewer" -rgba 0xffffffff

	sprite_viewer_action 0
}

proc sprite_viewer_action {action} {
	variable title
	variable title_pos
	variable max_sprites

	if {$action == 1} {incr title_pos -1}
	if {$action == 2} {incr title_pos  1}

	if {$title_pos > $max_sprites} {set title_pos 0}
	if {$title_pos < 0} {set title_pos $max_sprites}

	#show sprite matrix
	show_sprite $title_pos

	#fade/ease
	osd configure sprite_viewer.title.text -text "Sprite $title_pos" -fadeCurrent 0
	osd configure sprite_viewer.index      -text "In mem: $max_sprites"
	ease_text sprite_viewer.title.text 0 $action
}

proc sprite_viewer_hide {} {
	osd destroy sprite_viewer
	deactivate_input_layer sprite_viewer
}

proc ease_text {osd_object {frame_render 0} {action 0}} {
	#Ease in length
	set x 16

	#Intro Fade
	if {$frame_render == 0} {
	   osd configure $osd_object -x $x -fadeTarget 1 -fadePeriod 0.25
	}

	if {$frame_render > $x} {
		#leave routine
		return ""
	}

	#ease in
	if {$action == 2} {
		osd configure $osd_object -x [expr {$frame_render - $x}]
	} else {
		osd configure $osd_object -x [expr {$x - $frame_render}]
	}

	incr frame_render 4

	#call same function
	after realtime 0.05 [namespace code [list ease_text $osd_object $frame_render $action]]
}

proc show_sprite {sprite} {
	variable max_sprites

	set sprite_size [expr {([vdpreg 1] & 2) ? 16 : 8}]
	set factor [expr {($sprite_size == 8) ? 1 : 4}]
	set max_sprites [expr {(256 / $factor) - 1}]
	if {($sprite > $max_sprites) || ($sprite < 0)} {
		error "Please choose a value between 0 and $max_sprites"
	}

	osd destroy sprite_viewer.sprite_osd
	draw_matrix "sprite_viewer.sprite_osd" 7 56 7 $sprite_size 1

	set addr [expr {([vdpreg 6] << 11) + ($sprite * $factor * 8)}]
	for {set y 0} {$y < $sprite_size} {incr y; incr addr} {
		set pattern [vpeek $addr]
		set mask [expr {($sprite_size == 8) ? 0x80 : 0x8000}]
		if {$sprite_size == 16} {
			set pattern [expr {($pattern << 8) + [vpeek [expr {$addr + 16}]]}]
		}
		for {set x 0} {$x < $sprite_size} {incr x} {
			if {$pattern & $mask} {
				osd configure sprite_viewer.sprite_osd.${x}_${y} -rgba 0xffffffff
			}
			set mask [expr {$mask >> 1}]
		}
	}
}

proc draw_matrix {matrixname x y blocksize matrixsize matrixgap} {
	osd create rectangle $matrixname \
		-x $x \
		-y $y \
		-h [expr {$matrixsize * $blocksize}] \
		-w [expr {$matrixsize * $blocksize}] \
		-rgba 0x00000040

	for {set x 0} {$x < $matrixsize} {incr x} {
		for {set y 0} {$y < $matrixsize} {incr y} {
			osd create rectangle $matrixname.${x}_${y} \
				-h [expr {$blocksize - $matrixgap}] \
				-w [expr {$blocksize - $matrixgap}] \
				-x [expr {$x * $blocksize}] \
				-y [expr {$y * $blocksize}] \
				-rgba 0x0000ff80
		}
	}

	return ""
}

variable sprite_locator_enabled false
variable sprite_locator_after_id 0
variable frame_position 0.0

set_help_text toggle_sprite_locator \
"Enables/disables showing the locations of all 32 sprites according to the current VDP state.\n
Optionally, you can pass a parameter that indicates the fraction of the frame
to sample the VRAM data (default 0.0, start of frame). This is usually
necessary if there are screen splits or other raster effects used."

proc toggle_sprite_locator {{rel_frame_position 0.0}} {
	variable sprite_locator_enabled
	variable sprite_locator_after_id
	variable frame_position

	if {$rel_frame_position < 0.0 || $rel_frame_position >= 1.0} {
		error "Please use a value for rel_frame_position in the \[0.0, 1.0> range."
	}

	set frame_position $rel_frame_position

	if {$sprite_locator_enabled} {
		set sprite_locator_enabled false
		osd destroy sprite_locator
		after cancel $sprite_locator_after_id
		message "Sprite locator disabled"
	} else {
		set sprite_locator_enabled true
		osd_widgets::msx_init sprite_locator

		set position_max 40.0
		for {set i 0} {$i < 32} {incr i} {
			osd create rectangle sprite_locator.box$i -relx -100 -rely -100 -relh 8 -relw 8 \
				-rgba 0xFF111140 -borderrgba 0xFF1111C0 -bordersize 1
			# position text on X depending on the sprite number, to avoid overlapping
			# the idea is that most sprites for overlapping will have subsequent pattern numbers...
			set j [expr {($i % 2 == 0) ? $i : (31 - $i)}]
			osd create text sprite_locator.box$i.snr -text $i   -size 6 -y -6 -relx [expr $j/$position_max] -rgba 0xFFFFFFFF
			osd create text sprite_locator.box$i.pnr -text "00" -size 4 -y  0 -relx [expr $j/$position_max] -rgba 0xFFFFFFFF
		}
		set sprite_locator_after_id [after frame [namespace code locator_frame_callback]]
		message "Sprite locator enabled, showing red rectangles around sprites..."
	}
}

proc read_sat {base_mask index_mask index} {
	vpeek [expr {($index_mask | $index) & $base_mask}]
}

proc locator_frame_callback {} {
	variable frame_position
	variable sprite_locator_after_id
	set offset [expr {[vdp::get_frame_duration] * $frame_position}]
	set sprite_locator_after_id [after time $offset [namespace code update_locator]]
}

proc update_locator {} {
	variable sprite_locator_enabled
	if {!$sprite_locator_enabled} return

	osd_widgets::msx_update sprite_locator

	# parameters that depend on sprite mode (mode 0 handled below)
	set mode [get_screen_mode_number]
	set base_mask [expr {([vdpreg 11] << 15) | ([vdpreg 5] << 7) | 0x7f}]
	set index_mask [expr {($mode < 4) ? 0x1ff80 : 0x1fc00}]
	set index      [expr {($mode < 4) ? 0 : 0x200}]
	set special_y  [expr {($mode < 4) ? 208 : 216}]
	set lines [expr {([vdpreg 9] & 0x80) ? 212 : 192}]

	# parameters that depend on sprite shape (8x8 vs 16x16, magnified or not)
	set r1 [vdpreg 1]
	set pattern_mask [expr {($r1 & 2) ? 0xfc : 0xff}]
	set size [expr {8 * (($r1 & 2) ? 2 : 1) * (($r1 & 1) ? 2 : 1)}]

	# VDP draws sprites 1 pixel lower than the coordinate in VRAM
	# and take the vertical scroll register 23 into account
	set adjust_y [expr {1 - [vdpreg 23]}]

	set next_are_hidden [expr {($mode == 0) || ([vdpreg 8] & 2)}]
	# note that when all sprites are hidden, we still need the following
	# loop to (possibly) hide the widgets from the previous frame
	for {set i 0} {$i < 32} {incr i; incr index 4} {
		if {$next_are_hidden} {
			set y -100
			set x -100
		} else {
			set y [read_sat $base_mask $index_mask $index]
			if {$y == $special_y} {
				set next_are_hidden true
				set x -100
				set y -100
			} else {
				set y [expr {($y + $adjust_y) & 0xff}]
				if {$y > 224} { incr y -256 } ; # wrap to the top
				if {($y <= -$size) || ($y >= $lines)} { set y -100 } ; # hide invisible
				set x [read_sat $base_mask $index_mask [expr {$index | 1}]]
				set pnr [expr {[read_sat $base_mask $index_mask [expr {$index | 2}]] & $pattern_mask}]
				set attr_index [expr {($mode < 4) ? ($index | 3) : (16 * $i)}]
				set attr [read_sat $base_mask $index_mask $attr_index]
				if {$attr & 0x80} { incr x -32 }
				osd configure sprite_locator.box$i.pnr -text [format %02X $pnr]
			}
		}
		osd configure sprite_locator.box$i -relx $x -rely $y -relw $size -relh $size
	}

	variable sprite_locator_after_id
	set sprite_locator_after_id [after frame [namespace code locator_frame_callback]]
}

namespace export toggle_sprite_locator
namespace export sprite_viewer

} ;# namespace osd_sprite_info

namespace import osd_sprite_info::*
