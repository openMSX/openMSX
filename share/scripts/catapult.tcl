# Catapult Helper File (for use with openMSX Catapult)

# Catapult Helper File (for use with openMSX Catapult)

proc getScreen {} {
	#screen detection

	if {[peek 0xfcaf]>1} {return [concat "Screen Mode " [peek 0xfcaf] " Not Supported"]}

	if {[peek 0xfcaf]==0} 	{
				#screen 0
					if {[debug read VDP\ regs 0] & 0x04} {set chars 80} else {set chars 40}
					set base 0
				} else {
				#screen 1
					set base 6144
					set chars 32

				}

	set line ""
	set screen ""

	#scrape screen and build string
		for {set y 0} {$y < 23} {incr y } {
			set row [expr {$y * $chars}]
			for {set x 0} {$x < $chars} {incr x } {
				append line  [format %c [debug read VRAM [expr {$base + $x + $row}]]]
			}
		append screen [string trim $line]
		append screen "\n"
		set line ""
		}

	return  [string trim $screen]
}
