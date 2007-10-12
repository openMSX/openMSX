# Catapult Helper File (for use with openMSX Catapult)

proc getScreen {} {
set line ""
set screen ""
set base 0
	for {set y 0} {$y < 23} {incr y } {
		set base [expr {$y * [peek 0xf3b0]}]
		for {set x 0} {$x < [peek 0xf3b0]} {incr x } {
			append line  [format %c [debug read VRAM [expr {$x + $base}]]]
		}
		append screen [string trim $line]
		append screen "\n"
		set line ""
	}
return  [string trim $screen]
}

