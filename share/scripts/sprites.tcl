#attributes
#expr ([vdpreg 11] << 15) + (([vdpreg 5] & 0xf8) << 7)

proc draw_matrix {matrixname x y blocksize matrixsize matrixgap} {
	osd create rectangle $matrixname \
		-x $x \
		-y $y \
		-h [expr $matrixsize * $blocksize] \
		-w [expr $matrixsize * $blocksize] \
		-rgba 0xffffff40

	for {set x 0} {$x < $matrixsize} {incr x} {
		for {set y 0} {$y < $matrixsize} {incr y} {
			osd create rectangle $matrixname.${x}_${y} \
				-h [expr $blocksize - $matrixgap] \
				-w [expr $blocksize - $matrixgap] \
				-x [expr $x * $blocksize] \
				-y [expr $y * $blocksize] \
				-rgba 0x0000ffff
		}
	}
	return $matrixname
}

proc show_sprite {sprite} {
	set sprite_size [expr ([vdpreg 1] & 2) ? 16 : 8]
	set factor [expr ($sprite_size == 8) ? 1 : 4]
	set max_sprites [expr (256 / $factor) - 1]
	if {($sprite > $max_sprites) || ($sprite < 0)} {
		error "Please choose a value between 0 and $max_sprites"
	}

	catch { osd destroy sprite_osd }
	draw_matrix "sprite_osd" 16 16 3 $sprite_size 0

	set addr [expr ([vdpreg 6] << 11) + ($sprite * $factor * 8)]
	for {set y 0} {$y < $sprite_size} {incr y; incr addr} {
		set pattern [vpeek $addr]
		set mask [expr ($sprite_size == 8) ? 0x80 : 0x8000]
		if {$sprite_size == 16} {
			set pattern [expr ($pattern << 8) + [vpeek [expr $addr + 16]]]
		}
		for {set x 0} {$x < $sprite_size} {incr x} {
			if {$pattern & $mask} {
				osd configure sprite_osd.${x}_${y} -rgba 0xffffffff
			}
			set mask [expr $mask >> 1]
		}
	}
}
