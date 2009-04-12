# $Id$

# TODO: add a way to turn this off...

namespace eval text_echo {

set_help_text text_echo \
{Echoes all characters printed in text mode to stderr, meaning they will appear on the command line that openMSX was started from.
}

variable graph
variable escape
variable escape_count

proc text_echo {} {
	variable graph
	variable escape
	variable escape_count
	set graph 0
	set escape 0
	set escape_count 0
	debug set_bp 0x0018 {} { text_echo::print }
	debug set_bp 0x00A2 {} { text_echo::print }
	return ""
}

proc print {} {
	variable graph
	variable escape
	variable escape_count

	set slot [ get_selected_slot 0 ]
	if { $slot == "0 0" || $slot == "0 X" } {
		set char [ reg A ]
		if { $graph } {
			#puts stderr [ format "\[G%x\]" $char ] nonewline
			set graph 0
		} elseif { $escape } {
			#puts stderr [ format "\[E%x\]" $char ] nonewline
			if { $escape_count == 0 } {
				if { $char == 0x59 } {
					set escape_count 2
				} else {
					set escape_count 1
				}
			} else {
				incr escape_count -1
				if { $escape_count == 0 } {
					set escape 0
				}
			}
		} elseif { $char == 0x01 } {
			set graph 1
		} elseif { $char == 0x1B } {
			set escape 1
		} elseif { $char == 0x0A || $char >= 0x20 } {
			puts stderr [ format %c $char ] nonewline
		} else {
			#puts stderr [ format "\[N%x\]" $char ] nonewline
		}
	}
}

namespace export text_echo

} ;# namespace text_echo

namespace import text_echo::*
