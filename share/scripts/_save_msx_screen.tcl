set_help_text save_msx_screen \
{Lookup the screen mode and save the current screen to a MSX compatible binary
file. This file can for example be loaded in MSX-BASIC using the BLOAD command.

This script was originally developed by NYYRIKKI, see also this forum thread:
  http://www.msx.org/forum/msx-talk/general-discussion/taking-sc5-snapshot-games
}

proc save_msx_screen {basename} {
	set directory [file normalize $::env(OPENMSX_USER_DATA)/../screenshots]
	# Create filename with correct extension (also gives an error in
	# case of an invalid screen mode).
	set fname [file join $directory [format "%s.SC%X" $basename [get_screen_mode_number]]]

	set name_base        [expr { [vdpreg 2]        * 0x400}]
	set name_base_80     [expr {([vdpreg 2] & 252) * 0x400}]
	set name_base_bitmap [expr {([vdpreg 2] &  96) * 0x400}]
	set color_base       [expr { [vdpreg 3]        * 0x40 + [vdpreg 10] * 0x4000}]
	set color_base_2     [expr {([vdpreg 3] & 128) * 0x40 + [vdpreg 10] * 0x4000}]
	set pattern_base     [expr { [vdpreg 4]        * 0x800}]
	set pattern_base_2   [expr {([vdpreg 4] & 60)  * 0x800}]
	set spr_att_base     [expr { [vdpreg 5]        * 0x80 + [vdpreg 11] * 0x8000}]
	set spr_att_base_2   [expr {([vdpreg 5] & 252) * 0x80 + [vdpreg 11] * 0x8000 - 0x200}]
	set spr_pat_base     [expr { [vdpreg 6]        * 0x800}]

	switch [get_screen_mode] {
	"TEXT40" {
		lappend sections "VRAM" $name_base 0x400                    ;# BG Map
		lappend sections "VDP palette" 0 0x20                       ;# Palette
		lappend sections "VRAM" [expr {$name_base + 0x420}] 0x3E0   ;# Fill (BG Map)
		lappend sections "VRAM" $pattern_base 0x800                 ;# BG Tiles
	}
	"TEXT80" {
		lappend sections "VRAM" $name_base_80 0x800                 ;# BG Map
		lappend sections "VRAM" $color_base 0x700                   ;# Blink
		lappend sections "VDP palette" 0 0x20                       ;# Palette
		lappend sections "VRAM" [expr {$name_base_80 + 0xF20}] 0xE0 ;# Fill (BG Map)
		lappend sections "VRAM" $pattern_base 0x800                 ;# BG Tiles
	}
	"1" {
		lappend sections "VRAM" $pattern_base 0x1800                ;# BG Tiles
		lappend sections "VRAM" $name_base 0x300                    ;# BG Map
		lappend sections "VRAM" $spr_att_base 0x500                 ;# OBJ Attributes
		lappend sections "VRAM" $color_base 0x20                    ;# BG Colors
		lappend sections "VDP palette" 0 0x20                       ;# Palette
		lappend sections "VRAM" [expr {$color_base + 0x40}] 0x17C0  ;# Fill (BG Colors)
		lappend sections "VRAM" $spr_pat_base 0x800                 ;# OBJ Tiles
	}
	"2" {
		lappend sections "VRAM" $pattern_base_2 0x1800              ;# BG Tiles
		lappend sections "VRAM" $name_base 0x300                    ;# BG Map
		lappend sections "VRAM" $spr_att_base 0x80                  ;# OBJ Attributes
		lappend sections "VDP palette" 0 0x20                       ;# Palette
		lappend sections "VRAM" [expr {$spr_att_base + 0xA0}] 0x460 ;# Fill (OBJ Attributes)
		lappend sections "VRAM" $color_base_2 0x1800                ;# BG Colors
		lappend sections "VRAM" $spr_pat_base 0x800                 ;# OBJ Tiles
	}
	"3" {
		lappend sections "VRAM" $pattern_base 0x800                 ;# BG Tiles
		lappend sections "VRAM" $name_base 0x1300                   ;# BG Map
		lappend sections "VRAM" $spr_att_base 0x520                 ;# OBJ Attributes
		lappend sections "VDP palette" 0 0x20                       ;# Palette
		lappend sections "VRAM" [expr {$spr_att_base + 0x540}] 0x17C0 ;# Fill (OBJ Attributes)
		lappend sections "VRAM" $spr_pat_base 0x800                 ;# OBJ Tiles
	}
	"4" {
		lappend sections "VRAM" $pattern_base_2 0x1800              ;# BG Tiles
		lappend sections "VRAM" $name_base 0x380                    ;# BG Map
		lappend sections "VDP palette" 0 0x20                       ;# Palette
		lappend sections "VRAM" [expr {$name_base + 0x3A0}] 0x60    ;# Fill (BG Map)
		lappend sections "VRAM" $spr_att_base_2 0x400               ;# OBJ Attributes
		lappend sections "VRAM" $color_base_2 0x1800                ;# BG Colors
		lappend sections "VRAM" $spr_pat_base 0x800                 ;# OBJ Tiles
	}
	"5" - "6" {
		set lines1 [expr {[vdpreg 23] > 24 ? 256 - [vdpreg 23] : 232}] ;# Store 232 lines, even
		set lines2 [expr {232 - $lines1}]                              ;# though only 212 are visible
		set base1 [expr {$name_base_bitmap + [vdpreg 23] * 128}]
		set base2        $name_base_bitmap
		lappend sections "VRAM" $base1 [expr {$lines1 * 128}]       ;# Bitmap (part 1)
		lappend sections "VRAM" $base2 [expr {$lines2 * 128}]       ;# Bitmap (part 2)
		lappend sections "VRAM" $spr_att_base_2 0x280               ;# Sprite colors + attributes
		lappend sections "VDP palette" 0 0x20                       ;# Palette
		lappend sections "VRAM" [expr {$name_base_bitmap + 0x76A0}] 0x160 ;# Fill (Bitmap)
		lappend sections "VRAM" $spr_pat_base 0x800                 ;# Sprite character patterns
	}
	"7" - "8" - "11" - "12" {
		set lines1 [expr {[vdpreg 23] > 16 ? 256 - [vdpreg 23] : 240}] ;# Store 240 lines, even
		set lines2 [expr {240 - $lines1}]                              ;# though only 212 are visible
		set base1 [expr {($name_base_bitmap * 2) & 0x10000 + [vdpreg 23] * 256}]
		set base2 [expr {($name_base_bitmap * 2) & 0x10000 }]
		lappend sections "VRAM" $base1 [expr {$lines1 * 256}]       ;# Bitmap (part 1)
		lappend sections "VRAM" $base2 [expr {$lines2 * 256}]       ;# Bitmap (part 2)
		lappend sections "VRAM" $spr_pat_base 0x800                 ;# Sprite character patterns
		lappend sections "VRAM" $spr_att_base_2 0x280               ;# Sprite colors + attributes
		lappend sections "VDP palette" 0 0x20                       ;# Palette
	}}

	# open file
	set out [open $fname w]
	fconfigure $out -translation binary

	# write header
	set end_addr -1
	foreach {type addr size} $sections {
		incr end_addr $size
	}
	set header [list 0xFE 0 0 [expr {$end_addr & 255}] [expr {$end_addr / 256}] 0 0]
	puts -nonewline $out [binary format c* $header]

	# write data sections
	foreach {type addr size} $sections {
		puts -nonewline $out [debug read_block $type $addr $size]
	}
	close $out
	return "Screen written to $fname"
}
