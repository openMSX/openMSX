namespace eval tas {

set_help_text toggle_frame_counter\
{Toggles display of a frame counter in the lower right corner.

The frame counter will be wrong if at any time there is a change of VDP
register 9 bit 1 during the run time of the machine! (So switching between
50/60Hz.) In practice this means that it only works on machines that stay in
NTSC mode (because PAL machines with V9938 or up will start in NTSC mode for a
brief amount of time).
}

proc toggle_frame_counter {} {

	if {![catch {osd info framecount}]} {
		osd destroy framecount
		return ""
	}

	osd create rectangle framecount -x 280 -y 235 -h 6 -w 50 -rgba 0x0077FF80 -scaled true
	osd create text framecount.text -text "" -size 5 -rgba 0xffffffff

	framecount_update
	return ""
}

proc framecount_current {} {
	set freq [expr {(6 * 3579545) / (1368 * (([vdpreg 9] & 2) ? 313 : 262))}]
	return [expr int([machine_info time] * $freq)]
}

proc framecount_update {} {
	if {[catch {osd info framecount}]} {return ""}
	osd configure framecount.text -text "Frame: [framecount_current]"
	after frame [namespace code framecount_update]
	return ""
}

set_help_text advance_frame \
{Emulates until the next frame is generated and then pauses openMSX. Useful to
bind to a key and emulate frame by frame.}

proc advance_frame {} {
	after frame "set ::pause on"
	set ::pause off
	return ""
}

# more TODO:
# real time OSD memory watch
# TAS mode indicators

namespace export toggle_frame_counter
namespace export advance_frame
}

namespace import tas::*
