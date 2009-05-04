namespace eval vdp {

set_help_text getcolor \
{Return the current V99x8 palette settings for the given color index (0-15).
The result is format as RGB, with each component in the range 0-7.
}
proc getcolor { index } {
	set rb [debug read "VDP palette" [expr 2 * $index]]
	set g  [debug read "VDP palette" [expr 2 * $index + 1]]
	format "%03x" [expr (($rb * 16) & 0x700) + (($g * 16) & 0x070) + ($rb & 0x007)]
}

set_help_text setcolor \
{Change the V99x8 palette settings. See also getcolor.

usage:
  setcolor <index> <r><g><b>
   <index>   0-15
   <r><g><b> 0-7
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

proc format_table { entries columns frmt sep func } {
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

set_help_text vdpreg "Read or write a V99x8 register."
proc vdpreg {reg {value ""}} {
	if {$value eq ""} {
		debug read "VDP regs" $reg
	} else {
		debug write "VDP regs" $reg $value
	}
}

set_help_text vdpregs "Gives an overview of the V99x8 registers."
proc vdpregs { } {
	format_table 32 4 "%2d : 0x%02x" "   " vdpreg
}

set_help_text v9990reg "Read or write a V9990 register."
proc v9990reg {reg {value ""}} {
	if {$value eq ""} {
		debug read "Sunrise GFX9000 regs" $reg
	} else {
		debug write "Sunrise GFX9000 regs" $reg $value
	}
}

set_help_text v9990regs "Gives an overview of the V9990 registers."
proc v9990regs { } {
	format_table 55 5 "%2d : 0x%02x" "   " v9990reg
}

set_help_text palette "Gives an overview of the V99x8 palette registers."
proc palette { } {
	format_table 16 4 "%x:%s" "  " getcolor
}

proc val2bin val {
	set binRep [binary format c $val]
	binary scan $binRep B* binStr
	return $binStr
}

variable mode_lookup

set mode_lookup(00000000) 1;# Screen 1
set mode_lookup(00000001) 0;# Screen 0 (WIDTH 40)
set mode_lookup(00000010) 3;# Screen 3
set mode_lookup(00000100) 2;# Screen 2
set mode_lookup(00001000) 4;# Screen 4
set mode_lookup(00001001) 0;# Screen 0 (WIDTH 80)
set mode_lookup(00001100) 5;# Screen 5
set mode_lookup(00010000) 6;# Screen 6
set mode_lookup(00010100) 7;# Screen 7
set mode_lookup(00011100) 8;# Screen 8

set_help_text get_screen_mode "Decodes the current screen mode from the VDP registers (as would be used in the BASIC SCREEN command)"
proc get_screen_mode {} {
	variable mode_lookup
	set val [expr (([vdpreg 0] & 14) << 1) | (([vdpreg 1] & 8) >> 2) |  (([vdpreg 1] & 16) >> 4)]
	if {![info exists mode_lookup([val2bin $val])]} {
		error "Illegal VDP mode!"
		return -1
	}
	set mode $mode_lookup([val2bin $val])
	if {$mode == 8} {
		if {[expr [vdpreg 25] & 8]} {
			if {[expr [vdpreg 25] & 16]} {
				set mode 11
			} else {
				set mode 12
			}
		}
	}
	return $mode
}

set_help_text get_color_count "Gives some statistics on the used colors of the currently visible screen. Does not take into account sprites, screen splits and other trickery.

Options:
    -sort <field>, where <field> is either color (default) or amount
    -reverse, to reverse the sorting order
"
proc get_color_count {args} {

	set sortindex 0
	set sortorder "-increasing"

	# parse options
	while (1) {
		switch -- [lindex $args 0] {
		"-sort" {
			set sortfield [lindex $args 1]
			if {$sortfield == "color"} {
				set sortindex 0
			} elseif {$sortfield == "amount"} {
				set sortindex 1
			} else {
				error "Unsupported sort field $sortfield"
			}
			set args [lrange $args 2 end]
		}
		"-reverse" {
			set sortorder "-decreasing"
			set args [lrange $args 1 end]
		}
		"default" break
		}
	}

	set mode [get_screen_mode]
	if { $mode < 5 || $mode > 8 } {
		error "Screen mode $mode not supported (yet)"
	}
	set page [expr (([debug read "VDP regs" 2] & 96) >> 5)]
	if { [expr ([debug read "VDP regs" 9] & 128)] == 0} {
		set noflines 192
	} else {
		set noflines 212
	}
	puts "Counting pixels of screen $mode, page $page with $noflines lines..."
	
	set nofbytes_per_line 256
	switch $mode {
		5 {
			set nofbytes_per_line 128
			set nofpixels_per_byte 2
			set page_size [expr 32*1024]
		}
		6 {
			set nofbytes_per_line 128
			set nofpixels_per_byte 4
			set page_size [expr 32*1024]
		}
		7 {
			set nofpixels_per_byte 2
			set page_size [expr 64*1024]
		}
		8 {
			set nofpixels_per_byte 1
			set page_size [expr 64*1024]
		}
	}
	set bpp [expr 8 / $nofpixels_per_byte]

	# get bytes into large list
	set offset [expr $page_size * $page]
	set nrbytes [expr $noflines * $nofbytes_per_line]
	binary scan [debug read_block VRAM $offset $nrbytes] c* myvram

	# analyze pixels
	for {set p 0} { $p < 256 } {incr p} {
		set pixelstats($p) 0
	}
	foreach byte $myvram {
		for { set pixel 0} { $pixel < $nofpixels_per_byte} { incr pixel } {
			set color [expr {($byte >> ($pixel * (8 / $nofpixels_per_byte))) & ((1 << $bpp) - 1)} ]
			incr pixelstats($color)
		}
	}
	# convert to list
	set pixelstatlist [list]
	foreach key [array names pixelstats] {
		if { $pixelstats($key) != 0 } {
			lappend pixelstatlist [list $key $pixelstats($key)]
		}
	}
	set pixelstatlistsorted [lsort -integer $sortorder -index $sortindex $pixelstatlist]
	# print results
	set number 0
	foreach pixelinfo $pixelstatlistsorted {
		incr number
		set color [lindex $pixelinfo 0]
		set amount [lindex $pixelinfo 1]
		set palette ""
		set colorwidth 3
		if {$mode != 8} {
			set palette " ([getcolor $color])"
			set colorwidth 2
		}
		set colorformat [format "%${colorwidth}d" $color]
		set amountformat [format "%6d" $amount]
		set numberformat [format "%${colorwidth}d" $number]
		puts "$numberformat: color $colorformat$palette: amount: $amountformat"
	}
}

namespace export getcolor
namespace export setcolor
namespace export get_color_count
namespace export get_screen_mode
namespace export vdpreg
namespace export vdpregs
namespace export v9990regs
namespace export palette

} ;# namespace vdp

namespace import vdp::*
