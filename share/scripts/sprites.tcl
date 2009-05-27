#attributes
#expr ([debug read VDP\ regs 11] << 15) + ([debug read VDP\ regs 5] & 0xf8) << 7

proc draw_matrix {matrixname x y blocksize matrixsize matrixgap} {

	osd create rectangle $matrixname \
		-x $x\
		-y $y\
		-h [expr $matrixsize*$blocksize]\
		-w [expr $matrixsize*$blocksize]\
		-rgba 0xffffff40

	for {set x 0} {$x < $matrixsize} {incr x} {
		for {set y 0} {$y < $matrixsize} {incr y} {
			osd create rectangle $matrixname.[expr $x+$y*$matrixsize]\
				-h [expr $blocksize-$matrixgap]\
				-w  [expr $blocksize-$matrixgap]\
				-x [expr $x*$blocksize]\
				-y [expr $y*$blocksize]\
				-rgba 0x0000ffff
		}
	}
}


proc show_sprite {sprite} {

	if {[expr [debug read VDP\ regs 1] &3] < 2} {set sprite_size 8} else {set sprite_size 16}
	if {$sprite_size == 16} {set max_sprites 63} else {set max_sprites 255}

 	if {($sprite > $max_sprites) || $sprite < 0} {error "Please choose a value between 0 and $max_sprites"}

	if [info exists ::osd_sprite_overlay] {
		osd destroy sprite_osd
		unset ::osd_sprite_overlay
	}

	set ::osd_sprite_overlay [draw_matrix "sprite_osd" 16 16 3 $sprite_size 0]

	set addr [expr [debug read VDP\ regs 6] << 11]
	set addr [expr $addr+($sprite*($sprite_size*2))]

	for {set y 0} {$y < $sprite_size} {incr y} {
		set pos 0
			for {set mask 0x80} { $mask != 0} { set mask [expr $mask >> 1] } {
					if {[vpeek [expr $addr+$y]] & $mask} {
						osd configure sprite_osd.[expr $y*$sprite_size+$pos] -rgba 0xffffffff
					}
					if {$sprite_size>8} {
						if {[vpeek [expr $addr+$y+$sprite_size]] & $mask} {
						osd configure sprite_osd.[expr $y*$sprite_size+($pos+8)] -rgba 0xffffffff
					}
			}
			incr pos 1
		}
	}
}
