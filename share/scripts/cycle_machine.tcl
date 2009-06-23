namespace eval cycle_machine {

set help_cycle_machine \
{Cycle through the currently running machines.
'cycle_back_machine' does the same as 'cycle_machine', but it goes in the
opposite direction.
}

catch {osd destroy machine_indicator}

osd create rectangle machine_indicator \
		-x 1 \
		-y 1 \
		-alpha 0xff \
		-image [data_file scripts/olb.png] \
		-clip true

osd create text machine_indicator.text \
		-x 1 \
		-y 3 \
		-rgba 0xffffffff \
		-size 18

set_help_text cycle_machine $help_cycle_machine
set_help_text cycle_back_machine $help_cycle_machine

proc cycle_machine { {step 1} } {
	set cycle_list [list_machines]
	set cur [lsearch -exact $cycle_list [activate_machine]]
	set new [expr ($cur + $step) % [llength $cycle_list]]
	activate_machine [lindex $cycle_list $new]
	update_machine_indicator
}

proc cycle_back_machine {} {
        cycle_machine -1
}

proc update_machine_indicator {} {
		array set names [openmsx_info machines [machine_info config_name]]
		set mtime [machine_info time]
		set text [format "%s %s (%02d:%02d:%02d)" $names(manufacturer) $names(code) [expr int($mtime / 3600)] [expr int($mtime / 60) % 60] [expr int($mtime) % 60]]
		osd configure machine_indicator.text -text $text -alpha 255
		osd configure machine_indicator -alpha 0x80
		pop_menu machine_indicator
	}

proc pop_menu {osd_object {frame_render 0}} {

	#Ease in length
	set x 16

	#Intro Fade
	if {$frame_render==0} {
	   osd configure $osd_object -x 6 -fadeTarget 0x000000ff -fadePeriod 0.50
	   osd configure $osd_object.text -x $x -fadeTarget 0xffffffff -fadePeriod 0.25
	}

	#exit fade
	if {$frame_render>$x} {
		after time 2 [namespace code [list fade_menu $osd_object]]
		#leave routine
		return ""
	}

	#Text ease in
	osd configure $osd_object -x 0
	osd configure $osd_object.text -x [expr $x-$frame_render]
	incr frame_render 1

	#call same function
	after frame [namespace code [list pop_menu $osd_object $frame_render]]
}

proc fade_menu {osd_object} {
	osd configure $osd_object -fadeTarget 0x00000000 -fadePeriod 1
	osd configure $osd_object.text -fadeTarget 0xffffff00 -fadePeriod 1
}

after machine_switch update_machine_indicator

# keybindings
bind_default CTRL+PAGEUP cycle_machine
bind_default CTRL+PAGEDOWN cycle_back_machine

namespace export cycle_machine
namespace export cycle_back_machine
namespace export update_machine_indicator

} ;# namespace cycle_machine

namespace import cycle_machine::*





# This implementation is more robust for checking non existing machines.
# It also gives back a detailed list of running machines.

#toggle_machine 1
#toggle_machine -1

#--- test code ---
#create_machine
#create_machine
#create_machine
#create_machine
#create_machine
#create_machine

#proc list_all_machines {} {
#
#                set machine_list [list_machines]
#                set machine_current [machine]
#
#                foreach machine $machine_list {
#                                set active_machine ""
#                                if {$machine_current==$machine} {set active_machine "\[Active\]"}
#                                puts "$machine $active_machine"
#                }
#}

#proc toggle_machine {val} {
#                set machine_list [list_machines]
#                set machine_current [machine]

#               set machine_count 0
#                set active_machine 0

#                foreach machine $machine_list {
#                                set sel_machine($machine_count) $machine
#                                if {$machine_current==$machine} {set active_machine $machine_count}
#                                incr machine_count
#                }

#                set activate [expr $val+$active_machine]

#                if {[catch {activate_machine $sel_machine($activate)} errmsg]} {
#                                return "Machine does not exist yet"
#                }

#return "[machine]/[expr $machine_count]  [list_all_machines]"

#}
