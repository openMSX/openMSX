namespace eval save_debuggable {

set_help_text save_debuggable \
{Save a debuggable to file. Examples of debuggables are memory, VRAM, ...
Use 'debug list' to get a complete list of all debuggables.

Usage:
  save_debuggable VRAM vram.raw    Save the content of the MSX VRAM to a file
                                   called 'vram.raw'
}
proc save_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set data [debug read_block $debuggable 0 $size]
	set file [open $filename "WRONLY CREAT TRUNC"]
	fconfigure $file -translation binary -buffersize $size
	puts -nonewline $file $data
	close $file
}

set_help_text load_debuggable \
{Load a raw data file into a certain debuggable (see also save_debuggable).
Note that saving and reloading the same data again does not always bring the
MSX in the same state (e.g. the subslotregister). Use 'savestate' and 'loadstate'
if you want that.
}
proc load_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set file [open $filename "RDONLY"]
	fconfigure $file -translation binary -buffersize $size
	set data [read $file]
	close $file
	debug write_block $debuggable 0 $data
}

set_tabcompletion_proc save_debuggable [namespace code tab_loadsave_debuggable]
set_tabcompletion_proc load_debuggable [namespace code tab_loadsave_debuggable]
proc tab_loadsave_debuggable { args } {
	if {[llength $args] == 2} {
		return [debug list]
	}
}

# TODO remove these two procs?
#  They were meant as a very quick-and-dirty savestate mechanism, but it
#  doesn't work (e.g. because of subslot register)
proc save_all { directory } {
	puts stderr "This command ('save_all') has been deprecated (and may be removed in a future release). In contrast to what you might think, it cannot be used to save the machine state. You probably just want to use 'save_state' instead!"
	foreach debuggable [debug list] {
		save_debuggable $debuggable ${directory}/${debuggable}.sav
	}
}
proc load_all { directory } {
	puts stderr "This command ('load_all') has been deprecated (and may be removed in a future release). In contrast to what you might think, it cannot be used to load the machine state. You probably just want to use 'load_state' instead!"
	foreach debuggable [debug list] {
		load_debuggable $debuggable ${directory}/${debuggable}.sav
	}
}

# for backwards compatibility
proc vramdump { { filename "vramdump"} } {
	#puts stderr "This command ('vramdump') has been deprecated (and may be removed in a future release), please use 'save_debuggable VRAM' instead!"
	# I prefer to keep this command as a convenience shortcut
	save_debuggable "VRAM" $filename
}

set_help_text vram2bmp \
{Dump a selected region of the video memory into a bitmap file.

vram filename filename [start address] [width] [length]

usage example: vram2bmp RawDataDump.bmp 0 256 1024
}

proc vram2bmp {filename start dx dy} {
	set file [open $filename "WRONLY CREAT TRUNC"]
	set data_len [expr $dx * $dy / 2]
	set file_len [expr $data_len + 0x76]
	set file_len_HI [expr $file_len / 0x100]
	set dx_HI [expr $dx / 0x100]
	set dy_HI [expr $dy / 0x100]
	set data_len_HI [expr $data_len / 0x100]
	fconfigure $file -translation binary -buffersize $file_len

	puts -nonewline $file "\x42\x4d"                ;# BMP FILE identificator
	puts -nonewline $file [format %c $file_len]     ;# <-File length
	puts -nonewline $file [format %c $file_len_HI]  ;# <-File length
	puts -nonewline $file "\x00\x00"                ;# last always always zero because maximim file length is 128KB + header length
	puts -nonewline $file "\x00\x00\x00\x00"        ;# unused
	puts -nonewline $file "\x76\x00\x00\x00"        ;# offset to bitmap data - always same for 16 colors bmp
	puts -nonewline $file "\x28\x00\x00\x00"        ;# number of bytes in the header
	puts -nonewline $file [format %c $dx]           ;# bitmap width
	puts -nonewline $file [format %c $dx_HI]        ;# bitmap width
	puts -nonewline $file "\x00\x00"                ;# bitmap width - unused because maximum width is 256
	puts -nonewline $file [format %c $dy]           ;# bitmap height
	puts -nonewline $file [format %c $dy_HI]        ;# bitmap height
	puts -nonewline $file "\x00\x00"                ;# bitmap height, if negative image will look normal in mspaint
	puts -nonewline $file "\x01\x00"                ;# 1 plane used
	puts -nonewline $file "\x04\x00"                ;# 4 bits per pixel
	puts -nonewline $file "\x00\x00\x00\x00"        ;# no compression used
	puts -nonewline $file [format %c $data_len]     ;# RAW image data length
	puts -nonewline $file [format %c $data_len_HI]  ;# RAW image data length
	puts -nonewline $file "\x00\x00"                ;# RAW image data length - always zero, because maximum length is 128KB
	puts -nonewline $file "\x00\x00\x00\x00"
	puts -nonewline $file "\x00\x00\x00\x00"
	puts -nonewline $file "\x00\x00\x00\x00"        ;#how many colors used
	puts -nonewline $file "\x00\x00\x00\x00"        ;#important colors

	#set colors (16 colors [0..15])
	for {set col 0} {$col < 16} { incr col} {
		set color [getcolor $col]
		puts -nonewline $file [format %c [expr [string index  $color 2]*255/7]]
		puts -nonewline $file [format %c [expr [string index  $color 1]*255/7]]
		puts -nonewline $file [format %c [expr [string index  $color 0]*255/7]]
		puts -nonewline $file  "\x00"
	}

	set cur_addr $start
	for {set i 0} {$i < $dy} {incr i} {
		for {set addr $cur_addr} { $addr < [expr $cur_addr + $dx / 2] } { incr addr } {
			puts -nonewline $file [format %c [vpeek $addr]]
		}
		set cur_addr [expr $cur_addr + 0x80 ]
	}
	close $file
}

namespace export save_debuggable
namespace export load_debuggable
namespace export save_all
namespace export load_all
namespace export vramdump
namespace export vram2bmp

} ;# namespace save_debuggable

namespace import save_debuggable::*
