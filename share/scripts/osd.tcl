set_help_text show_osd \
{Give an overview of all currently defined OSD elements and their properties.
This is mainly useful to debug a OSD related script.}

proc show_osd {{widgets ""}} {
	set result ""
	if {$widgets == ""} {
		# all widgets
		set widgets [osd info]
	}
	foreach widget $widgets {
		append result "$widget\n"
		foreach property [osd info $widget] {
			append result "  $property [osd info $widget $property]\n"
		}
	}
	return $result
}
