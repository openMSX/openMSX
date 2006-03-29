proc getcolor { index } {
	set rb [debug read "VDP palette" [expr 2 * $index]]
	set g  [debug read "VDP palette" [expr 2 * $index + 1]]
	format "%03x" [expr (($rb * 16) & 0x700) + (($g * 16) & 0x070) + ($rb & 0x007)]
}

proc setcolor { index rgb } {
	if [catch {
		if {[string length $rgb] != 3 } error
		set r [string index $rgb 0]
		set g [string index $rgb 1]
		set b [string index $rgb 2]
		if {($index < 0) || ($index > 15)} error
		if {($r > 7) || ($g > 7) || ($b > 7)} error
		debug write "VDP palette" [expr $index * 2] [expr $r * 16 + $b]
		debug write "VDP palette" [expr $index * 2 + 1] $g
	}] {
		error "Usage: setcolor <index> <rgb>\n index 0..15\n r,g,b 0..7"
	}
}

proc __format_table { entries columns frmt sep func } {
	set rows [expr ($entries + $columns - 1) / $columns]
	for {set row 0} { $row < $rows } { incr row } {
		set line ""
		for { set col 0 } { $col < $columns } { incr col } {
			set index [expr $row + ($col * $rows)]
			if { $index < $entries } {
				append line [format $frmt $index [$func $index]] $sep
			}
		}
		puts $line
	}
}

set_help_text vdpregs "Gives an overview of the V99x8 registers."
proc vdpreg { reg } {
	debug read "VDP regs" $reg
}
proc vdpregs { } {
	__format_table 32 4 "%2d : 0x%02x" "   " vdpreg
}

set_help_text v9990regs "Gives an overview of the V9990 registers."
proc v9990reg { reg } {
	debug read "Sunrise GFX9000 regs" $reg
}
proc v9990regs { } {
	__format_table 55 5 "%2d : 0x%02x" "   " v9990reg
}

set_help_text palette "Gives an overview of the V99x8 palette registers."
proc palette { } {
	__format_table 16 4 "%x:%s" "  " getcolor
}
