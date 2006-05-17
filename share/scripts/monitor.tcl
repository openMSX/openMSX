set_help_text monitor_type \
{Select a monitor color profile.

Usage:
  monitor_type          Shows the current profile
  monitor_type -list    Show all possible types
  monitor_type <type>   Select new type
}

set_tabcompletion_proc monitor_type tab_monitor_type
proc tab_monitor_type { args } {
	set result [array names ::__monitor]
	lappend result "-list"
}

set __monitor(normal) {{ 1 0 0 } { 0 1 0 } { 0 0 1 }}
set __monitor(red)    {{ 1 0 0 } { 0 0 0 } { 0 0 0 }}
set __monitor(green)  {{ 0 0 0 } { 0 1 0 } { 0 0 0 }}
set __monitor(blue)   {{ 0 0 0 } { 0 0 0 } { 0 0 1 }}
set __monitor(monochrome_amber) {{ .257 .504 .098 } { .193 .378 .073 } { 0 0 0 }}
set __monitor(monochrome_amber_bright) {{ .333 .333 .333 } { .249 .249 .249 } { 0 0 0 }}
set __monitor(monochrome_green) {{ .129 .252 .049 } { .257 .504 .098 } { .129 .252 .049 }}
set __monitor(monochrome_green_bright) {{ .167 .167 .167 } { .333 .333 .333 } { .167 .167 .167 }}
set __monitor(monochrome_white) {{ .257 .504 .098 } { .257 .504 .098 } { .257 .504 .098 }}
set __monitor(monochrome_white_bright) {{ .333 .333 .333 } { .333 .333 .333 } { .333 .333 .333 }}

proc monitor_type { {monitor ""} } {
	if [string equal $monitor ""] {
		foreach type [array names ::__monitor] {
			if [string equal $::__monitor($type) $::color_matrix] {
				return $type
			}
		}
		return "Custom monitor type: $::color_matrix"
	} elseif [string equal $monitor "-list"] {
		return [array names ::__monitor]
	} else {
		if [info exists ::__monitor($monitor)] {
			set ::color_matrix $::__monitor($monitor)
		} else {
			error "No such monitor type: $monitor"
		}
	}
}
