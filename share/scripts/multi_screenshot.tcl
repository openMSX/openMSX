# Take multiple screenshots
#
# Usage:
#  multi_screenshot <num> [<base>]
#
proc multi_screenshot {num {base ""} } {
	multi_screenshot_helper 1 $num $base
	return ""
}

proc multi_screenshot_helper {acc max {base ""} } {
	if {$acc <= $max} {
		if {$base == ""} { screenshot } { screenshot -prefix ${base} }
		after frame "multi_screenshot_helper [expr $acc + 1] $max $base"
	}
}
