namespace eval type_from_file {

proc tabcompletion_normal {args} {
	set possibilities [utils::file_completion {*}$args]
	if {[llength $args] > 2} {
		lappend possibilities {-release} {-freq} ;# duplicated from C++
	}
	return $possibilities
}

set_tabcompletion_proc type_from_file [namespace code tabcompletion_normal]

set_help_text type_from_file \
{type_from_file filename <type args>
Types the content of the indicated file into the MSX.
If you want to specify extra arguments for the normal type command,
you have to specify them after the filename.
}

proc type_from_file {filename args} {

	# open file
	set f [open $filename "r"]

	# process all lines in the file
	while {[gets $f line] >= 0} {
		type {*}$args "$line\r"
	}

	close $f
}

proc tabcompletion_password {args} {
	utils::file_completion {*}$args
}

set_tabcompletion_proc type_password_from_file [namespace code tabcompletion_password]

set_help_text type_password_from_file \
{type_password_from_file filename <index>
Types the content of a single line from the indicated file.
If index is specified, the line at that index is typed. The first line of
the file has index 1. Blank lines and lines starting with # are not counted.

This command is useful to type in passwords which you have stored in a file.

Example of an input file:

# ------------------
# Usas
# ------------------
# 1
juba ruins
# 2
harappa ruins
# 3
gandhara ruins
# 4
mohenjo daro

If this file is saved as 'usas.txt', the 3rd password can be type like this:
type_password_from_file usas.txt 3
}

proc type_password_from_file {filename {index 1}} {

	# open file
	set f [open $filename "r"]

	set line_index 1
	set found false
	# process all lines in the file
	while {[gets $f line] >= 0} {
		if {[string first "#" $line] == 0} continue ;# skip lines with #
		if {[string length $line] == 0} continue ;# skip blank lines
		if {$line_index == $index} {
			type -release $line
			set found true
			break
		}
		incr line_index
	}

	close $f

	if {!$found} { error "No line found at index $index" }
}

# type_from_file_via_keybuf
# Based on NYYRIKKI's code found here:
# https://www.msx.org/forum/msx-talk/openmsx/lost-somewhere-inside-openmsx-please-save-me?page=0

proc tabcompletion_type_from_file_via_keybuf {args} {
	utils::file_completion {*}$args
}

set_tabcompletion_proc type_from_file_via_keybuf [namespace code tabcompletion_type_from_file_via_keybuf]

set_help_text type_from_file_via_keybuf \
{This is an alternative to type_from_file. It's a lot faster, but it only works
in software that reads the input from the keyboard buffer area in the RAM. In
MSX-BASIC, this one works very well. It simply pokes the bytes from the file
directly into the keyboard buffer.
}

variable bp_id

proc type_from_file_via_keybuf {filename} {
	variable bp_id

	set fid [open $filename r]
	fconfigure $fid -translation binary -encoding binary
	set bp_id [debug set_bp 0x38 {} "type_from_file::handleinterrupt $fid"]
	return ""
}


proc handleinterrupt {fid} {
	set PUTPNT 0xF3F8
	set GETPNT 0xF3FA
	set KEYBUF 0xFBF0
	set BUFEND 0xFC18

	if {[peek16 $PUTPNT] == [peek16 $GETPNT]} {
		# keyboard buffer is processed

		set addr $KEYBUF
		
		# poke the buffer (almost!) full with the next file content
		while {$addr < $BUFEND - 1} {
			set char [read $fid 1]
			if {$char eq ""} {
				# apparently eof reached, clean up
				variable bp_id
				debug remove_bp $bp_id
				close $fid
				break
			}
			# skip newlines
			if {$char ne "\x0A"} {
				poke $addr [scan $char "%c"]
				incr addr
			}
		}
		# put the end of the data pointer to addr
		poke16 $PUTPNT $addr
		# put read pointer at start of keyboard buffer
		poke16 $GETPNT $KEYBUF
	}	
}

namespace export type_from_file
namespace export type_password_from_file
namespace export type_from_file_via_keybuf

} ;# namespace type_from_file

namespace import type_from_file::*
