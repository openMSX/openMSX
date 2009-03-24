set_help_text load_icons \
{Load a different set of OSD icons.
 usage:  load_icons <name> [<position>]
          <name> is the name of a directory (share/skins/<name>) that
          contains the icon images
          <position> can be one of the following 'bottom', 'top',
          'left' or 'right'. Default is 'bottom'
 example: load_icons set1 top
}

set_tabcompletion_proc load_icons __tab_load_icons
proc __tab_load_icons { args } {
	set num [llength $args]
	if {$num == 2} {
		set r1 [glob -nocomplain -tails -type d -directory $::env(OPENMSX_USER_DATA)/skins *]
		set r2 [glob -nocomplain -tails -type d -directory $::env(OPENMSX_SYSTEM_DATA)/skins *]
		join [list $r1 $r2]
	} elseif {$num == 3} {
		list "top" "bottom" "left" "right"
	}
}

proc __trace_icon_status { name1 name2 op } {
	global $name1 __last_change
	set icon [string trimleft $name1 ":"]
	set __last_change($icon) [openmsx_info realtime]
	__redraw_osd_icons $icon
}

proc __redraw_osd_icons { icon } {
	global __fade_delay_active __fade_delay_non_active __last_change __fade_id $icon

	# handle 'unset' variables  (when current msx machine got deleted)
	if [catch {set value [set $icon]}] { set value false }

	if $value {
		set widget  osd_icons.${icon}_on
		set widget2 osd_icons.${icon}_off
		set fade_delay $__fade_delay_active($icon)
	} else {
		set widget  osd_icons.${icon}_off
		set widget2 osd_icons.${icon}_on
		set fade_delay $__fade_delay_non_active($icon)
	}
	osd configure $widget2 -alpha 0 -fadeTarget 0

	if {$fade_delay == 0} {
		# no fading yet
		osd configure $widget -alpha 255 -fadeTarget 255
	} else {
		set diff [expr [openmsx_info realtime] - $__last_change($icon)]
		if {$diff < $fade_delay} {
			# no fading yet
			osd configure $widget -alpha 255 -fadeTarget 255
			catch {
				after cancel $__fade_id($icon)
			}
			set __fade_id($icon) [after realtime [expr $fade_delay - $diff] "__redraw_osd_icons $icon"]
		} else {
			# fading out
			osd configure $widget -fadeTarget 0
		}
	}
}

proc load_icons { set_name { set_position "" } } {
	# Check skin directory
	#  All files belonging to this skin must come from this directory.
	#  So we don't allow mixing individual files for one skin from the
	#  system and from the user directory. Though fallback images are
	#  again searched in both system and user dirs.
	set skin_set_dir [data_file "skins/$set_name"]
	if {![file isdirectory $skin_set_dir]} {
		error "No such icon skin: $set_name"
	}

	# Check position
	if {$set_position == ""} {
		# keep current position
		set set_position $::osd_leds_pos
	}
	if {($set_position != "top") &&
	    ($set_position != "bottom") &&
	    ($set_position != "left") &&
	    ($set_position != "right")} {
		error "Invalid position: $set_position"
	}

	# Defaut icon positions
	set xbase 0
	set ybase 0
	set xwidth 50
	set yheight 30
	set xspacing 60
	set yspacing 35
	set horizontal 1
	set fade_delay 5
	set fade_duration 5
	set scale 2

	# but allow to override these values by the skin script
	set icons $::__icons  ;# the 'none' skin needs this
	set script $skin_set_dir/skin.tcl
	if [file isfile $script] { source $script }

	set invscale [expr 1.0 / $scale]
	set xbase    [expr $xbase    * $invscale]
	set ybase    [expr $ybase    * $invscale]
	set xwidth   [expr $xwidth   * $invscale]
	set yheight  [expr $yheight  * $invscale]
	set xspacing [expr $xspacing * $invscale]
	set yspacing [expr $yspacing * $invscale]

	# change according to <position> parameter
	if { $set_position == "left" } {
		set horizontal 0
	} elseif { $set_position == "right" } {
		set horizontal 0
	        set xbase [ expr 320 - $xwidth]
	} elseif { $set_position == "bottom" } {
	        set ybase [ expr 240 - $yheight ]
	}
	set vertical [expr !$horizontal]

	proc __try_dirs { skin_set_dir file fallback } {
		# don't touch already resolved pathnames
		if [file isfile $file] { return $file }
		# first look in specified skin-set directory
		set f1 $skin_set_dir/$file
		if [file isfile $f1] { return $f1 }
		# if it's not there look in the root skin directory
		# (system or user directory)
		set f2 [data_file "skins/$file"]
		if [file isfile $f2] { return $f2 }
		# still not found, look for the fallback image in system and
		# user root skin dir
		set f3 [data_file "skins/$fallback"]
		if [file isfile $f3] { return $f3 }
		return ""
	}
	# Calculate default parameter values ...
	for { set i 0 } { $i < [llength $icons] } { incr i } {
		set icon [lindex $icons $i]

		set xcoord($icon) [expr $i * $xspacing * $horizontal]
		set ycoord($icon) [expr $i * $yspacing * $vertical]

		set fade_delay_active($icon)     $fade_delay
		set fade_delay_non_active($icon) $fade_delay

		set fade_duration_active($icon)     $fade_duration
		set fade_duration_non_active($icon) $fade_duration

		switch -glob $icon {
			led_* {
				set base [string tolower [string range $icon 4 end]]
				set image_on     "${base}-on.png"
				set image_off    "${base}-off.png"
				set fallback_on  "led-on.png"
				set fallback_off "led-off.png"
			}
			throttle {
				set image_on     ""
				set image_off    "${icon}.png"
				set fallback_on  ""
				set fallback_off ""
				set fade_delay_non_active($icon) 0
			}
			default {
				set image_on     "${icon}.png"
				set image_off    ""
				set fallback_on  ""
				set fallback_off ""
				set fade_delay_active($icon) 0
			}
		}
		set active_image($icon)     [__try_dirs $skin_set_dir $image_on  $fallback_on ]
		set non_active_image($icon) [__try_dirs $skin_set_dir $image_off $fallback_off]
	}

	# ... but allow to override these calculated values (again) by the skin script
	if [file isfile $script] { source $script }

	osd configure osd_icons -x $xbase -y $ybase

	foreach icon $::__icons {
		osd configure osd_icons.${icon}_on \
		       -x $xcoord($icon) \
		       -y $ycoord($icon) \
		       -fadePeriod $fade_duration_active($icon) \
		       -image [__try_dirs $skin_set_dir $active_image($icon) ""] \
		       -scale $invscale
		osd configure osd_icons.${icon}_off \
		       -x $xcoord($icon) \
		       -y $ycoord($icon) \
		       -fadePeriod $fade_duration_non_active($icon) \
		       -image [__try_dirs $skin_set_dir $non_active_image($icon) ""] \
		       -scale $invscale
	}

	# Also try to load "frame.png"
	catch { osd destroy osd_frame }
	set framefile "$skin_set_dir/frame.png"
	if [file isfile $framefile] {
		osd create rectangle osd_frame -z 0 -x 0 -y 0 -w 320 -h 240 \
		                               -scaled true -image $framefile
	}

	# If successful, store in settings (order of assignments is important!)
	set ::__osd_leds_set $set_name
	set ::osd_leds_set $set_name
	set ::__osd_leds_pos $set_position
	set ::osd_leds_pos $set_position
	foreach icon $::__icons {
		set ::__fade_delay_active($icon)     $fade_delay_active($icon)
		set ::__fade_delay_non_active($icon) $fade_delay_non_active($icon)
	}

	return ""
}

proc __trace_osd_icon_vars {name1 name2 op} {
	# avoid executing load_icons multiple times
	# (because of the assignments to the settings in that proc)
	if {($::osd_leds_set == $::__osd_leds_set) &&
	    ($::osd_leds_pos == $::__osd_leds_pos)} {
		return
	}
	load_icons $::osd_leds_set $::osd_leds_pos
}

proc __machine_switch_osd_icons {} {
	foreach icon $::__icons {
		trace remove variable ::$icon "write unset" __trace_icon_status
		trace add    variable ::$icon "write unset" __trace_icon_status
		__redraw_osd_icons $icon
	}
	after machine_switch __machine_switch_osd_icons
}

# Available icons. Icons are also drawn in this order (by default)
set __icons [list "led_power" "led_caps" "led_kana" "led_pause" "led_turbo" "led_FDD" \
                  "pause" "throttle" "mute" "breaked"]

# create OSD widgets
osd create rectangle osd_icons -scaled true -alpha 0 -z 1
foreach __icon $__icons {
	osd create rectangle osd_icons.${__icon}_on  -alpha 0 -fadeTarget 0 -fadePeriod 5.0
	osd create rectangle osd_icons.${__icon}_off -alpha 0 -fadeTarget 0 -fadePeriod 5.0
	trace add variable ::$__icon "write unset" __trace_icon_status
	set ::__last_change($__icon) [openmsx_info realtime]
}

# Restore settings from previous session
user_setting create string osd_leds_set "Name of the OSD icon set" set1
user_setting create string osd_leds_pos "Position of the OSD icons" bottom
set __osd_leds_set $osd_leds_set
set __osd_leds_pos $osd_leds_pos
trace add variable osd_leds_set write __trace_osd_icon_vars
trace add variable osd_leds_pos write __trace_osd_icon_vars
after machine_switch __machine_switch_osd_icons

load_icons $osd_leds_set $osd_leds_pos
