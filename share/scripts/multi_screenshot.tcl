set_help_text multi_screenshot \
{Take multiple screenshots

Usage:
 multi_screenshot <num> [<base>]
}

proc multi_screenshot {num {base ""} } {
	__multi_screenshot_helper 1 $num $base
	return ""
}

proc __multi_screenshot_helper {acc max {base ""} } {
	if {$acc <= $max} {
		if {$base == ""} { screenshot } { screenshot -prefix ${base} }
		after frame "__multi_screenshot_helper [expr $acc + 1] $max $base"
	}
}
