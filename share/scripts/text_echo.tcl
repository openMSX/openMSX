# $Id$

set_help_text text_echo \
{Echoes all characters printed in text mode to stderr, meaning they will appear on the command line that openMSX was started from.
}

proc text_echo {} {
	set ::text_echo_graph 0
	set ::text_echo_escape 0
	set ::text_echo_escape_count 0
	debug set_bp 0x0018 {} { text_echo_print }
	debug set_bp 0x00A2 {} { text_echo_print }
	return ""
}

proc text_echo_print { } {
	set slot [ get_selected_slot 0 ]
	if { $slot == "0 0" || $slot == "0 X" } {
		set char [ reg A ]
		if { $::text_echo_graph } {
			#puts stderr [ format "\[G%x\]" $char ] nonewline
			set ::text_echo_graph 0
		} elseif { $::text_echo_escape } {
			#puts stderr [ format "\[E%x\]" $char ] nonewline
			if { $::text_echo_escape_count == 0 } {
				if { $char == 0x59 } {
					set ::text_echo_escape_count 2
				} else {
					set ::text_echo_escape_count 1
				}
			} else {
				decr ::text_echo_escape_count
				if { $::text_echo_escape_count == 0 } {
					set ::text_echo_escape 0
				}
			}
		} elseif { $char == 0x01 } {
			set ::text_echo_graph 1
		} elseif { $char == 0x1B } {
			set ::text_echo_escape 1
		} elseif { $char == 0x0A || $char >= 0x20 } {
			puts stderr [ format %c $char ] nonewline
		} else {
			#puts stderr [ format "\[N%x\]" $char ] nonewline
		}
	}
}
