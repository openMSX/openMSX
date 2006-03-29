set_help_text load_icons \
{Load a different icon set (OSD LEDs)
 usage:  load_icons <name> <position>
          <name> is the name of a directory (share/skins/<name>) that
          contains the led images
          <position> can be one of the following 'bottom','top',
          'left' or 'right'. Default is 'top'
 example: load_icons set1 bottom
}
proc load_icons { set_name { set_position top } } {
	global renderer

	# Check skin directory
	set directory [data_file "skins/$set_name"]
	if { ![file exists $directory] } {
		error "No such LED skin"
	}
	
	# Available LEDs. LEDs are also drawn in this order (by default)
	set leds "power caps kana pause turbo fdd"
	
	# Defaut LED positions
	set xbase 0
	set ybase 0
	set xwidth 50
	set yheight 30
	set xspacing 60
	set yspacing 35
	set horizontal 1

	# but allow to override these values by the skin script
	set script $directory/skin.tcl
	if [file exists $script] {
		source $script
	}

	# and since SDLLo has almost no screen estate we deliberately
	# ingnore the spacing settings for this renderer
	if { "SDLLo" == $renderer } {
		set xspacing $xwidth
		set yspacing $yheight
	}

	# change according to <position> parameter
	if { $set_position == "left" } {
		set horizontal 0
	}

	if { $set_position == "right" } {
		set horizontal 0
		if { "SDLLo" == $renderer } {
			set xbase [ expr 320 - $xwidth ]
		} else {
			set xbase [ expr 640 - $xwidth ]
		}
	}

	if { $set_position == "bottom" } {
		if { "SDLLo" == $renderer } {
			set power off
			set ybase [ expr 240 - $yheight ]
		} else {
			set ybase [ expr 480 - $yheight ]
			reset
		}
	}

	# Default fade parameters
	set fade_delay 5000
	set fade_duration 5000

	# Calculate default parameter values ...
	for { set i 0 } { $i < [llength $leds] } { incr i } {
		set led [lindex $leds $i]
		set xcoord($led) [expr $xbase + $i * $xspacing * $horizontal]
		set ycoord($led) [expr $ybase + $i * $yspacing * [expr !$horizontal]]
		set active_fade_delay($led) $fade_delay
		set active_fade_duration($led) $fade_duration
		set non_active_fade_delay($led) $fade_delay
		set non_active_fade_duration($led) $fade_duration
		set active_image($led) ${led}-on.png
		set non_active_image($led) ${led}-off.png
	}

	# ... but allow to override these calculated values (again) by the skin script
	set script $directory/skin.tcl
	if [file exists $script] {
		source $script
	}

	proc set_image { setting directory file } {
		if [string equal $file ""] {
			set $setting ""
		} elseif [file exists $directory/$file] {
			set $setting $directory/$file
		}
	}
	foreach led $leds {
		set ::icon.${led}.xcoord $xcoord($led)
		set ::icon.${led}.ycoord $ycoord($led)
		set ::icon.${led}.active.fade-delay $active_fade_delay($led)
		set ::icon.${led}.active.fade-duration $active_fade_duration($led)
		set ::icon.${led}.non-active.fade-delay $non_active_fade_delay($led)
		set ::icon.${led}.non-active.fade-duration $non_active_fade_duration($led)
		set_image ::icon.${led}.active.image $directory $active_image($led)
		set_image ::icon.${led}.non-active.image $directory $non_active_image($led)
	}
	return ""
}
