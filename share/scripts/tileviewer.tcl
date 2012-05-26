proc checkclick {} {

if {![osd exists tile_viewer]} {
	unbind_default "mouse button1 down"
	return ""
}

	#check editor matrix
	for {set xm 0} {$xm < 8} {incr xm} {
		for {set ym 0} {$ym < 8} {incr ym} {
			foreach {x y} [osd info "tile_viewer.matrix.${xm}_${ym}" -mousecoord] {}
				if {($x>=0 && $x<=1)&&($y>=0 && $y<=1)} {
				
					#lets chop this up in small pieces
					set fc [osd info tile_viewer.fc${ym} -rgba]
					set bc [osd info tile_viewer.bc${ym} -rgba]
					set tc [osd info tile_viewer.matrix.${xm}_${ym} -rgba]
				
					#toggle switch
					if {$fc==$tc} \
					{osd configure tile_viewer.matrix.${xm}_${ym} -rgba $bc -bordersize 1 -borderrgba 0xffffff80 } \
					else \
					{osd configure tile_viewer.matrix.${xm}_${ym} -rgba $fc -bordersize 1 -borderrgba 0xffffff80 }
					
					set tile_add_val 0
					for {set i 0} {$i < 8} {incr i} {
						if {[osd info tile_viewer.matrix.${i}_${ym} -rgba]==$bc} {incr tile_add_val [expr 2**(7-$i)]}
					}
					
					vpoke [osd info tile_viewer.matrix.text$ym -text] $tile_add_val
					return ""
				
			}
		}
	}
}

proc showtile {tile} {

bind_default "mouse button1 down" {checkclick}

set addr [expr $tile*8]

	osd destroy tile_viewer
	osd create rectangle tile_viewer -x 30 -y 30 -h 180 -w 330 -rgba 0x00007080
	osd_sprite_info::draw_matrix "tile_viewer.matrix" 90 24 18 8 1
	osd create text tile_viewer.text -x 0 -y 0 -size 20 -text "Tile: $tile" -rgba 0xffffffff
	
	for {set i 0} {$i < 16} {incr i} {
		set color [getcolor $i]

		set r [string range $color 0 0]
		set g [string range $color 1 1]
		set b [string range $color 2 2]

		set rgbval($i) [expr ($r << (5 + 16)) + ($g << (5 + 8)) + ($b << 5)]
		#osd create rectangle tile_viewer.colorpicker$i -y 192 -x [expr 4+$i*22] -w 20 -h 20 -rgb $rgbval($i)
	}

	# Build colors
	for {set i 0} {$i < 8} {incr i} {
		
		if {[get_screen_mode]!=1} {set col [vpeek [expr $addr+ ((([vdpreg 10] << 8 | [vdpreg 3]) << 6) & 0x1e000) +$i]]} else {set col 0xf0}
		
		set bg [expr ($col & 0xf0) >> 4 ]
		set fc [expr $col & 0x0f]
		osd create rectangle tile_viewer.bc$i -x 242 -w 16 -y [expr 24+$i*18] -h 16 -rgb $rgbval($bg) -bordersize 1 -borderrgba 0xffffff80
		osd create rectangle tile_viewer.fc$i -x 260 -w 16 -y [expr 24+$i*18] -h 16 -rgb $rgbval($fc) -bordersize 1 -borderrgba 0xffffff80
		
		osd create text tile_viewer.matrix.text$i -x -50 -y [expr $i*18]  -text [format 0x%4.4x [expr $addr+ (([vdpreg 4] & 0x3c) << 11) +$i]] -rgba 0xffffffff
		osd create text tile_viewer.matrix.color$i -x 188 -y [expr $i*18]  -text [format 0x%2.2x $col] -rgba 0xffffffff
	}

	# Build patterns
	for {set y 0} {$y < 8} {incr y} {

			set pattern [vpeek [expr $addr+ (([vdpreg 4] & 0x3c) << 11) +$y]]
			set mask 0x80
		
			for {set x 0} {$x < 8} {incr x} {
				if {$pattern & $mask} {
					osd configure tile_viewer.matrix.${x}_${y} -rgba [osd info tile_viewer.bc$y -rgba] -bordersize 1 -borderrgba 0xffffff80
				} else {osd configure tile_viewer.matrix.${x}_${y} -rgba [osd info tile_viewer.fc$y -rgba] -bordersize 1 -borderrgba 0xffffff80 }
				set mask [expr $mask >> 1]
			}
	}
}


proc showall {} {
if {![osd exists all_tiles]} {

	osd create rectangle all_tiles -x 0 -y 0 -h 255 -w 255 -rgba 0xff0000ff

	for {set x 0} {$x < 256} {incr x} {
		for {set y 0} {$y < 64} {incr y} {
			osd create rectangle all_tiles.${x}_${y} -x $x -y $y -h 1 -w 1
			}
		}
}

set screen [get_screen_mode]

	for {set i 0} {$i < 16} {incr i} {
		set color [getcolor $i]

		set r [string range $color 0 0]
		set g [string range $color 1 1]
		set b [string range $color 2 2]

		set rgbval($i) [expr ($r << (5 + 16)) + ($g << (5 + 8)) + ($b << 5)]
	}

	for {set x 0} {$x < 32} {incr x} {
		for {set y 0} {$y < 8} {incr y} {
		
		for {set yt 0} {$yt <8} {incr yt} {	
		
		set mask 0x80
		set pattern [vpeek [expr (([vdpreg 4] & 0x3c) << 11)+($x*8)+($y*256)+$yt]]
		
		if {$screen>1} {set col [vpeek [expr ((([vdpreg 10] << 8 | [vdpreg 3]) << 6) & 0x1e000) +($x*8)+($y*256)+$yt]]} else {set col 0xf0}
		
		set bg [expr ($col & 0xf0) >> 4 ]
		set fc [expr $col & 0x0f]

				for {set xt 0} {$xt < 8} {incr xt} {
	
				set xpos [expr ($x*8)+$xt]
				set ypos [expr ($y*8)+$yt]
	
				incr pix
	
					if {$pattern & $mask} {
						osd configure all_tiles.${xpos}_${ypos}  -rgb $rgbval($bg) -alpha 255
					} else {osd configure all_tiles.${xpos}_${ypos} -rgb $rgbval($fc) -alpha 255}
					set mask [expr $mask >> 1]
					
				}
			}
		}
	}
}
