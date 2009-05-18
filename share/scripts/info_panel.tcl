# Shows an info panel similar to the blueMSX DIGIblue theme
#
# TODO:
# - make layout more flexible?
# - make sure it doesn't interfere with LEDs?

namespace eval info_panel {

variable info_panel_active false
variable textheight 15
variable panel_margin 2
variable sub_panel_height [expr $textheight * 2 + $panel_margin]
variable panel_info
variable panel_list [list software fps screen vram ram mapper mtime speed machine]

proc info_panel_init {} {
	variable info_panel_active
	variable textheight
	variable panel_margin
	variable sub_panel_height
	variable panel_info
	variable panel_list

	set panel_info(software,title) "Running software"
	set panel_info(software,width) 400;#whatever
	set panel_info(software,row) 0
	set panel_info(software,method) {guess_title}
	set panel_info(mapper,title) "Mapper type"
	set panel_info(mapper,width) 170
	set panel_info(mapper,row) 0
	set panel_info(mapper,method) {set val ""; catch {set val [openmsx_info romtype [lindex [machine_info device [guess_title]] 1]]}; set val}
	set panel_info(fps,title) "FPS"
	set panel_info(fps,width) 38
	set panel_info(fps,row) 1
	set panel_info(fps,method) {format "%2.1f" [expr (round([openmsx_info fps] * 10) / 10.0)]};# the * 10 and / 10.0 is for proper rounding on one decimal
	set panel_info(screen,title) "Screen"
	set panel_info(screen,width) 50
	set panel_info(screen,row) 1
	set panel_info(screen,method) {get_screen_mode}
	set panel_info(vram,title) "VRAM"
	set panel_info(vram,width) 42
	set panel_info(vram,row) 1
	set panel_info(vram,method) {format "%dkB" [expr [debug size "physical VRAM"] / 1024]};
	set panel_info(ram,title) "RAM"
	set panel_info(ram,width) 51
	set panel_info(ram,row) 1
	set panel_info(ram,method) {set ramsize 0; foreach device [debug list] {set desc [debug desc $device]; if {$desc == "memory mapper" || $desc == "ram"} {incr ramsize [debug size $device]}}; format "%dkB" [expr $ramsize / 1024]}
	set panel_info(mtime,title) "Time"
	set panel_info(mtime,width) 60
	set panel_info(mtime,row) 1
	set panel_info(mtime,method) {set mtime [machine_info time]; format "%02d:%02d:%02d" [expr int($mtime / 3600)] [expr int($mtime / 60) % 60] [expr int($mtime) % 60]}
	set panel_info(speed,title) "Speed"
	set panel_info(speed,width) 48
	set panel_info(speed,row) 1
	set panel_info(speed,method) {format "%d%%" [expr round([get_speed] * 100)]}
	set panel_info(machine,title) "Machine"
	set panel_info(machine,width) 250
	set panel_info(machine,row) 1
	set panel_info(machine,method) {array set names [openmsx_info machines [machine_info config_name]]; format "%s %s" $names(manufacturer) $names(code)}

	# calc width of software item
	set software_width 0
	foreach name $panel_list {
		if {$panel_info($name,row) == 1} {
			incr software_width $panel_info($name,width)
			incr software_width $panel_margin
		} else {
			if {$name != "software"} {
				incr software_width -$panel_info($name,width)
				incr software_width -$panel_margin
			}
		}
	}
	incr software_width -$panel_margin
	set panel_info(software,width) $software_width

	#set base element	
	osd create rectangle info_panel \
		-x $panel_margin \
		-y [expr -($sub_panel_height * 2 + (2 * $panel_margin))] \
		-rely 1.0 \
		-alpha 0

	#create subpanels
	set curpos(0) 0
	set curpos(1) 0
	foreach name $panel_list {
		set row $panel_info($name,row)
		create_sub_panel \
			$name \
			$panel_info($name,title) \
			$panel_info($name,width) \
			$panel_info($name,row) \
			$curpos($row)
		incr curpos($row) [expr $panel_info($name,width) + $panel_margin]
	}
}

proc create_sub_panel {name title width row pos} {
	variable textheight
	variable panel_margin
	variable sub_panel_height

	osd create rectangle info_panel.$name \
		-x $pos \
		-y [expr ($sub_panel_height + $panel_margin) * $row] \
		-h $sub_panel_height \
		-w $width \
		-rgba 0x00005080 \
		-clip true
	osd create text info_panel.$name.title \
		-x 2 \
		-y 0 \
		-rgba 0xddddffff \
		-text $title \
		-size [expr int($textheight * 0.8)]
	osd create text info_panel.$name.value \
		-x 2 \
		-y $textheight \
		-rgba 0xffffffff \
		-text $name \
		-size [expr int($textheight * 0.9)]
}

proc update_info_panel {} {
	variable info_panel_active
	variable panel_info
	variable panel_list

	if {!$info_panel_active} return

	foreach name $panel_list {
		osd configure info_panel.$name.value -text [eval $panel_info($name,method)]
	}
	after realtime 1 [namespace code update_info_panel]
}

proc toggle_info_panel {} {
	variable info_panel_active

	if {$info_panel_active} {
		catch {after cancel $vu_meter_trigger_id}
		set info_panel_active false
		osd destroy info_panel
	} else {
		set info_panel_active true
		info_panel_init
		update_info_panel
	}
}

## stuff to calculate the speed, which could be made public later

variable speed
if {[info command clock] == "clock"} {
	variable measurement_time 1.0;# in seconds
} else {
	# if the clock command doesn't exist, we have a broken (limited) Tcl
	# installation, which means we need to work around with a less accurate
	# measurement method. This method starts to get reasonably accurate using
	# longer measurement times, like 2.5 seconds.
	variable measurement_time 2.5;# in seconds
}
variable last_emutime 0
variable last_realtime 0

proc get_speed {} {
	variable speed
	return $speed
}

proc update_speed {} {
	variable speed
	variable measurement_time
	variable last_emutime
	variable last_realtime

	set new_emutime [machine_info time]
	if {[info command clock] == "clock"} {
		set new_realtime [clock clicks -millis]
		set speed [expr ($new_emutime - $last_emutime) / (($new_realtime - $last_realtime) / 1000.0)]
		set last_realtime $new_realtime
		#puts stderr [format "Realtime duration: %f, emutime duration: %f, speed ratio: %f" [expr (($new_realtime - $last_realtime) / 1000.0)] [expr ($new_emutime - $last_emutime)] [set speed]];# for debugging
	} else {
		set speed [expr ($new_emutime - $last_emutime) / $measurement_time]
	}
	set last_emutime $new_emutime
	after realtime $measurement_time [namespace code update_speed]
}

update_speed

namespace export toggle_info_panel

} ;# namespace info_panel

namespace import info_panel::*
