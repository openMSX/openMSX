namespace eval osd {

variable default_color "0x7090aae8 0xa0c0dde8 0x90b0cce8 0xc0e0ffe8"
variable error_color "0xaa0000e8 0xdd0000e8 0xcc0000e8 0xff0000e8"
variable warning_color "0xaa6600e8 0xdd9900e8 0xcc8800e8 0xffaa00e8"
variable message_counter 0

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

proc display_message {message {category info}} {
	variable message_counter
	variable default_color
	variable error_color
	variable warning_color
	switch -- $category {
		"info"     {set bg_color $default_color}
		"progress" {set bg_color $default_color}
		"warning"  {set bg_color $warning_color}
		"error"    {set bg_color $error_color  }
		"default"  {error "Invalid category: $category"}
	}

	set old_count $message_counter
	incr message_counter
	set new_name "osd_display_message_${message_counter}"

	osd_widgets::text_box $new_name \
		-text $message \
		-textrgba 0xffffffff \
		-textsize 6 \
		-rgba $bg_color \
		-x 3 -y 12 -z 5 -w 314 \
		-bordersize 0.5 \
		-borderrgba 0x000000ff \
		-clip true \
		-scaled true \
		-suppressErrors true

	set offset [expr {[osd info $new_name -h] + 1}]
	while {1} {
		set old_name "osd_display_message_${old_count}"
		if {![osd exists $old_name]} break
		osd configure $old_name -y [expr {$offset + [osd info $old_name -y]}]
		incr old_count -1
	}
}

proc is_cursor_in {widget} {
	set x 2; set y 2
	catch {lassign [osd info $widget -mousecoord] x y}
	expr {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1}
}

# only export stuff that is useful in other scripts or for the console user
namespace export show_osd
namespace export display_message
namespace export is_cursor_in

};# namespace osd

# only import stuff to global that is useful outside of scripts (i.e. for the console user)
namespace import osd::show_osd
