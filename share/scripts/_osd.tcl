namespace eval osd {

set_help_text show_osd \
{Give an overview of all currently defined OSD elements and their properties.
This is mainly useful to debug a OSD related script.}

proc show_osd {{widgets ""}} {
	set result ""
	if {$widgets eq ""} {
		# all widgets
		set widgets [osd info]
	}
	foreach widget $widgets {
		append result "$widget\n"
		foreach property [osd info $widget] {
			if {[catch {set value [osd info $widget $property]}]} {
				set value "--error--"
			}
			append result "  $property $value\n"
		}
	}
	return $result
}

proc is_cursor_in {widget} {
	set x 2; set y 2
	catch {lassign [osd info $widget -mousecoord] x y}
	expr {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1}
}

namespace export show_osd
namespace export is_cursor_in

};# namespace osd

# only import stuff to global that is useful outside of scripts (i.e. for the console user)
namespace import osd::show_osd
