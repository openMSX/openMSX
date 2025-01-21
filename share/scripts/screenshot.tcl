namespace eval openmsx {

proc screenshot {args} {
	set args2 [list]
	set sprites true
	foreach arg $args {
		if {$arg eq "-no-sprites"} {
			set sprites false
		} elseif {$arg eq "-guess-name"} {
			set base [utils::filename_clean [guess_title untitled]]
			if {$base ne ""} {lappend args2 -prefix "$base "}
		} else {
			lappend args2 $arg
		}
	}
	if {$sprites} {
		set filename [::openmsx::internal_screenshot {*}$args2]
		message "Screen saved to $filename"
	} else {
		# disable sprites, wait for one complete frame and take screenshot
		set orig_disable_sprites $::disablesprites
		set ::disablesprites true
		after frame [namespace code [list screenshot_helper1 $orig_disable_sprites $args2]]
	}
}
proc screenshot_helper1 {orig_disable_sprites args2} {
	after frame [namespace code [list screenshot_helper2 $orig_disable_sprites $args2]]
}
proc screenshot_helper2 {orig_disable_sprites args2} {
	# take screenshot and restore 'disablesprites' setting
	set filename [::openmsx::internal_screenshot {*}$args2]
	message "Screen saved to $filename"
	set ::disablesprites $orig_disable_sprites
}

set_help_text screenshot \
{screenshot                   Write screenshot to file "openmsxNNNN.png"
screenshot <filename>        Write screenshot to indicated file
screenshot -prefix foo       Write screenshot to file "fooNNNN.png"
screenshot -raw              320x240 raw screenshot (of MSX screen only)
screenshot -raw -doublesize  640x480 raw screenshot (of MSX screen only)
screenshot -with-osd         Include OSD elements in the screenshot
screenshot -no-sprites       Don't include sprites in the screenshot
screenshot -guess-name       Guess the name of the running software and use it as prefix
}

set_tabcompletion_proc screenshot [namespace code screenshot_tab]
proc screenshot_tab {args} {
	list "-prefix" "-raw" "-doublesize" "-with-osd" "-no-sprites" "-guess-name"
}

namespace export screenshot

}; # namespace

namespace import openmsx::screenshot
