# screen capture: optional sprite disable
# BiFi 2009

namespace eval screencapture {

set_help_text screencap \
{Capture the screen.

With the disablesprites boolean setting sprites can be disabled
during screencap. Very useful when creating maps.
}

variable cur_pp

proc screencap {} {
	after frame [namespace code init]
	return ""
}

proc init {} {
	variable cur_pp

	if {$::disablesprites} {
		vdpreg 8 [expr [vdpreg 8] | 0x02]
		set cur_pp [debug probe set_bp VDP.IRQhorizontal 1 [namespace code probe]]
	}
	after frame [namespace code callback]
	return ""
}

proc callback {} {
	variable cur_pp

	screenshot
	if {$::disablesprites} {
		vdpreg 8 [expr [vdpreg 8] & 0xfd]
		debug probe remove_bp $cur_pp
	}
}

proc probe {} {
	vdpreg 8 [expr [vdpreg 8] | 0x02]
	vpoke [expr [vdpreg 11] << 15 | ([vdpreg 5] & ([get_screen_mode_number]<4 ? 0xff : 0xfc)) << 7] [expr [get_screen_mode_number]<4 ? 208 : 216]
}

user_setting create boolean disablesprites "Disable sprites when doing a screencap" false

namespace export screencap

}; # namespace

namespace import screencapture::*
