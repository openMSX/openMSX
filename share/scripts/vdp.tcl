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
	set rows [expr $entries / $columns]
	for {set row 0} { $row < $rows } { incr row } {
		set line ""
		for { set col 0 } { $col < $columns } { incr col } {
			set index [expr $row + ($col * $rows)]
			append line [format $frmt $index [$func $index]] $sep
		}
		puts $line
	}
}

proc vdpreg { reg } {
	debug read "VDP regs" $reg
}
proc vdpregs { } {
	__format_table 32 4 "%2d : 0x%02x" "   " vdpreg
}

proc palette { } {
	__format_table 16 4 "%x:%s" "  " getcolor
}
