set_help_text about \
"Shows a list of commands that seem related to the given keyword.
If there is only one such command the helptext for that command is also shown."

proc about { cmd } {
	set matches [list]
	foreach command [info commands] {
		catch {
			if {[regexp -nocase -- $cmd [help $command]] ||
			    [string equal $command $cmd]} {
				lappend matches $command
			}
		}
	}
	if {[llength $matches] == 0} {
		error "No candidates found."
	} elseif {[llength $matches] == 1} {
		set match [lindex $matches 0]
		puts "$match:"
		puts [help $match]
	} else {
		puts "Multiple candidates found:"
		foreach match $matches {
			puts "  $match"
		}
	}
}
