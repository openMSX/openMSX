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

proc load_replay { name } {
	reverse loadreplay $name
	array set _i [reverse status]
	reverse goto $_i(end)
	set pause on
	return ""
}

set_help_text enable_tas_mode \
{Enables the highly experimental TAS mode, giving you 8 savestate slots, to be
used with F1-F8 (load) and SHIFT-F1 (to F8) to save. It actually saves
replays. After loading, openMSX will advance to the end of the replay
and pause. This will go slower and slower if you have longer replays.
(Will be improved after openMSX 0.8.0 is released.)

It will also enable the frame counter and give you a frame_advance key binding
under the END key.
}

proc enable_tas_mode {} {
	# assume frame counter is disabled here
	toggle_frame_counter
	reverse_widgets::enable_reversebar false

	# set up the quicksave/load "slots"
	set basename "quicksave"
	for {set i 1} {$i <= 8} {incr i} {
		bind_default "F$i" tas::load_replay "$basename$i"
		bind_default "SHIFT+F$i" "reverse savereplay $basename$i"
	}

	# set up frame advance
	bind_default END -repeat advance_frame

	# TODO:
	# enable extra stuff that Vampier wrote :)

	return "WARNING! TAS mode is still very experimental and will almost certainly change next release!"
}

namespace export toggle_frame_counter
namespace export advance_frame
namespace export enable_tas_mode
}

namespace import tas::*
