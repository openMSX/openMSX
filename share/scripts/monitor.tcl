namespace eval monitor {

variable monitors

set_help_text monitor_type \
{Select a monitor color profile.

Usage:
  monitor_type          Shows the current profile
  monitor_type -list    Show all possible types
  monitor_type <type>   Select new type
}

set_tabcompletion_proc monitor_type [namespace code tab_monitor_type]
proc tab_monitor_type { args } {
	variable monitors
	set result [array names monitors]
	lappend result "-list"
}

set monitors(normal) {{ 1 0 0 } { 0 1 0 } { 0 0 1 }}
set monitors(red)    {{ 1 0 0 } { 0 0 0 } { 0 0 0 }}
set monitors(green)  {{ 0 0 0 } { 0 1 0 } { 0 0 0 }}
set monitors(blue)   {{ 0 0 0 } { 0 0 0 } { 0 0 1 }}
set monitors(monochrome_amber) {{ .257 .504 .098 } { .193 .378 .073 } { 0 0 0 }}
set monitors(monochrome_amber_bright) {{ .333 .333 .333 } { .249 .249 .249 } { 0 0 0 }}
set monitors(monochrome_green) {{ .129 .252 .049 } { .257 .504 .098 } { .129 .252 .049 }}
set monitors(monochrome_green_bright) {{ .167 .167 .167 } { .333 .333 .333 } { .167 .167 .167 }}
set monitors(monochrome_white) {{ .257 .504 .098 } { .257 .504 .098 } { .257 .504 .098 }}
set monitors(monochrome_white_bright) {{ .333 .333 .333 } { .333 .333 .333 } { .333 .333 .333 }}

proc monitor_type { {monitor ""} } {
	variable monitors

	if {$monitor eq ""} {
		foreach type [array names monitors] {
			if {$monitors($type) eq $::color_matrix} {
				return $type
			}
		}
		return "Custom monitor type: $::color_matrix"
	} elseif {$monitor eq "-list"} {
		return [array names monitors]
	} else {
		if [info exists monitors($monitor)] {
			set ::color_matrix $monitors($monitor)
		} else {
			error "No such monitor type: $monitor"
		}
	}
}

namespace export monitor_type

} ;# namespace monitor

namespace import monitor::*
