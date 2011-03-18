set_help_text about \
"Shows a list of commands that seem related to the given keyword.
If there is only one such command the helptext for that command is also shown."

proc about {cmd} {
	set matches [list]
	foreach command [info commands] {
		catch {
			if {[regexp -nocase -- $cmd [help $command]] ||
			    ($command eq $cmd)} {
				lappend matches $command
			}
		}
	}
	set result ""
	if {[llength $matches] == 0} {
		error "No candidates found."
	} elseif {[llength $matches] == 1} {
		set match [lindex $matches 0]
		append result "$match:\n"
		append result "[help $match]\n"
	} else {
		append result "Multiple candidates found:\n"
		foreach match $matches {
			append result "  $match\n"
		}
	}
	return $result
}
