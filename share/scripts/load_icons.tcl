set_help_text load_icons \
{Load a different set of OSD LED icons.
 usage:  load_icons <name> [<position>]
          <name> is the name of a directory (share/skins/<name>) that
          contains the led images
          <position> can be one of the following 'bottom', 'top',
          'left' or 'right'. Default is 'bottom'
 example: load_icons set1 top
}

set_tabcompletion_proc load_icons tab_load_icons
proc tab_load_icons { args } {
	set num [llength $args]
	if {$num == 2} {
		set r1 [glob -nocomplain -tails -type d -directory $::env(OPENMSX_USER_DATA)/skins *]
		set r2 [glob -nocomplain -tails -type d -directory $::env(OPENMSX_SYSTEM_DATA)/skins *]
		join [list $r1 $r2]
	} elseif {$num == 3} {
		list "top" "bottom" "left" "right"
	}
}

proc __trace_led_status { name1 name2 op } {
	global $name1 __last_change
	set led [string trimleft $name1 ":"]
	set __last_change($led) [openmsx_info realtime]
	if [set $name1] {
		# off -> on
		osd configure osd_leds.${led}_off -alpha 0
	} else {
		# on -> off
		osd configure osd_leds.${led}_on  -alpha 0
	}
	__redraw_osd_leds
}

proc __redraw_osd_leds {} {
	# TODO split this for individual leds? -> make led induction var a parameter
	set fade_time 5.0
	set fade_duration 5.0

	set retrigger false
	foreach led $::__leds {
		set diff [expr [openmsx_info realtime] - $::__last_change(led_${led})]
		if {$fade_time == 0} {
			# no fading, draw completely opaque
			set alpha 255
		} elseif {$diff > ($fade_time + $fade_duration)} {
			# completely fade out
			set alpha 0
		} elseif {$diff < $fade_time} {
			# no fading yet
			set alpha 255
			set retrigger true
		} else {
			# fading out
			set alpha [expr 255 - (255 * ($diff - $fade_time) / $fade_duration)]
			set retrigger true
		}
		if [set ::led_${led}] {
			osd configure osd_leds.led_${led}_on  -alpha $alpha
		} else {
			osd configure osd_leds.led_${led}_off -alpha $alpha
		}
	}
	if $retrigger { after frame __redraw_osd_leds }
}

proc load_icons { set_name { set_position "" } } {
	# Check skin directory
	set directory [data_file "skins/$set_name"]
	if { ![file exists $directory] } {
		error "No such LED skin: $set_name"
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
	
	# Defaut LED positions
	set xbase 0
	set ybase 0
	set xwidth 50
	set yheight 30
	set xspacing 60
	set yspacing 35
	set horizontal 1
	set scale 2

	# but allow to override these values by the skin script
	set script $directory/skin.tcl
	if [file exists $script] { source $script }

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

	# Calculate default parameter values ...
	for { set i 0 } { $i < [llength $::__leds] } { incr i } {
		set led [lindex $::__leds $i]
		set led_lower [string tolower $led]

		set xcoord($led) [expr $i * $xspacing * $horizontal]
		set ycoord($led) [expr $i * $yspacing * $vertical]
		set active_image($led) ${led_lower}-on.png
		set non_active_image($led) ${led_lower}-off.png
	}

	# ... but allow to override these calculated values (again) by the skin script
	if [file exists $script] { source $script }

	osd configure osd_leds -x $xbase -y $ybase

	proc get_image { directory file } {
		if [file isfile $directory/$file] {
			return $directory/$file
		}
		return ""
	}
	foreach led $::__leds {
		osd configure osd_leds.led_${led}_on \
		       -x $xcoord($led) \
		       -y $ycoord($led) \
		       -image [get_image $directory $active_image($led)] \
		       -scale $invscale
		osd configure osd_leds.led_${led}_off \
		       -x $xcoord($led) \
		       -y $ycoord($led) \
		       -image [get_image $directory $non_active_image($led)] \
		       -scale $invscale
	}

	# If successful, store in settings (order of assignments is important!)
	set ::__osd_leds_set $set_name
	set ::osd_leds_set $set_name
	set ::__osd_leds_pos $set_position
	set ::osd_leds_pos $set_position

	return ""
}

proc __trace_osd_led_vars {name1 name2 op} {
	# avoid executing load_icons multiple times
	# (because of the assignments to the settings in that proc)
	if {($::osd_leds_set == $::__osd_leds_set) &&
	    ($::osd_leds_pos == $::__osd_leds_pos)} {
		return
	}
	osd_load_icons $::osd_leds_set $::osd_leds_pos
}

# Available LEDs. LEDs are also drawn in this order (by default)
set __leds "power caps kana pause turbo FDD"

# create OSD widgets
osd create rectangle osd_leds -scaled true -alpha 0
foreach led $__leds {
	osd create rectangle osd_leds.led_${led}_on  -alpha 0
	osd create rectangle osd_leds.led_${led}_off -alpha 0
	trace add variable ::led_${led} write __trace_led_status
	set ::__last_change(led_${led}) [openmsx_info realtime]
}

# Restore settings from previous session
user_setting create string osd_leds_set "Name of the OSD LED icon set" set1
user_setting create string osd_leds_pos "Position of the OSD LEDs" bottom
set __osd_leds_set $osd_leds_set
set __osd_leds_pos $osd_leds_pos
trace add variable osd_leds_set write __trace_osd_led_vars
trace add variable osd_leds_pos write __trace_osd_led_vars

osd_load_icons $osd_leds_set $osd_leds_pos
