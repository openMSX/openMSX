# Load a different icon set (OSD LEDs)
#  usage:  load_icons <name>
#           <name> is the name of a directory (share/skins/<name>) that
#           contains the led images
#  example: load_icons set1

proc load_icons { set_name } {

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
	set spacing 50
	set horizontal 1

	# Default fade parameters
	set fade_delay 5000
	set fade_duration 5000

	# Calculate default parameter values ...
	for { set i 0 } { $i < [llength $leds] } { incr i } {
		set led [lindex $leds $i]
		set xcoord($led) [expr $xbase + $i * $spacing * $horizontal]
		set ycoord($led) [expr $ybase + $i * $spacing * [expr !$horizontal]]
		set active_fade_delay($led) $fade_delay
		set active_fade_duration($led) $fade_duration
		set non_active_fade_delay($led) $fade_delay
		set non_active_fade_duration($led) $fade_duration
		set active_image($led) ${led}-on.png
		set non_active_image($led) ${led}-off.png
	}

	# ... but allow to override these values by the skin script
	set script $directory/skin.tcl
	if [file exists $script] {
		source $script
	}

	proc set_image { setting value } {
		if [file exists $value] {
			set $setting $value
		}
	}
	foreach led $leds {
		set ::icon.${led}.xcoord $xcoord($led)
		set ::icon.${led}.ycoord $ycoord($led)
		set ::icon.${led}.active.fade-delay $active_fade_delay($led)
		set ::icon.${led}.active.fade-duration $active_fade_duration($led)
		set ::icon.${led}.non-active.fade-delay $non_active_fade_delay($led)
		set ::icon.${led}.non-active.fade-duration $non_active_fade_duration($led)
		set_image ::icon.${led}.active.image $directory/$active_image($led)
		set_image ::icon.${led}.non-active.image $directory/$non_active_image($led)
	}
	return ""
}
