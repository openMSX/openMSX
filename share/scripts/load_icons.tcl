# Load a different icon set (OSD LEDs)
#  usage:  load_icons <name>
#           <name> is the name of a directory (share/skins/<name>) that
#           contains the led images
#  example: load_icons set1

namespace eval load_icons {

	# global fade delay in ms (0 is no fade out)
	set fade_delay 5000

	# global fade duration in ms
	set fade_duration 5000

	# LED positions
	set xbase 0
	set ybase 0
	set spacing 50
	set align_horizontal 1

	# LED order
	set order(power) 0
	set order(caps)  1
	set order(kana)  2
	set order(pause) 3
	set order(turbo) 4
	set order(fdd)   5

	proc offset { led } {
		expr $load_icons::order($led) * $load_icons::spacing
	}
	proc x_coord { led } {
		set result $load_icons::xbase
		if { $load_icons::align_horizontal } {
			incr result [offset $led]
		}
		return $result
	}
	proc y_coord { led } {
		set result $load_icons::ybase
		if { !$load_icons::align_horizontal } {
			incr result [offset $led]
		}
		return $result
	}

	proc load_icons { set_name } {
		set directory [data_file "skins/$set_name"]

		set ::icon.power.xcoord [x_coord power]
		set ::icon.power.ycoord [y_coord power]
		set ::icon.power.active.fade-delay $load_icons::fade_delay
		set ::icon.power.non-active.fade-delay $load_icons::fade_delay
		set ::icon.power.active.fade-duration $load_icons::fade_duration
		set ::icon.power.non-active.fade-duration $load_icons::fade_duration
		catch { set ::icon.power.active.image $directory/power-on.png }
		catch { set ::icon.power.non-active.image $directory/power-off.png }

		set ::icon.caps.xcoord [x_coord caps]
		set ::icon.caps.ycoord [y_coord caps]
		set ::icon.caps.active.fade-delay $load_icons::fade_delay
		set ::icon.caps.non-active.fade-delay $load_icons::fade_delay
		set ::icon.caps.active.fade-duration $load_icons::fade_duration
		set ::icon.caps.non-active.fade-duration $load_icons::fade_duration
		catch { set ::icon.caps.active.image $directory/caps-on.png }
		catch { set ::icon.caps.non-active.image $directory/caps-off.png }

		set ::icon.kana.xcoord [x_coord kana]
		set ::icon.kana.ycoord [y_coord kana]
		set ::icon.kana.active.fade-delay $load_icons::fade_delay
		set ::icon.kana.non-active.fade-delay $load_icons::fade_delay
		set ::icon.kana.active.fade-duration $load_icons::fade_duration
		set ::icon.kana.non-active.fade-duration $load_icons::fade_duration
		catch { set ::icon.kana.active.image $directory/kana-on.png }
		catch { set ::icon.kana.non-active.image $directory/kana-off.png }

		set ::icon.pause.xcoord [x_coord pause]
		set ::icon.pause.ycoord [y_coord pause]
		set ::icon.pause.active.fade-delay $load_icons::fade_delay
		set ::icon.pause.non-active.fade-delay $load_icons::fade_delay
		set ::icon.pause.active.fade-duration $load_icons::fade_duration
		set ::icon.pause.non-active.fade-duration $load_icons::fade_duration
		catch { set ::icon.pause.active.image $directory/pause-on.png }
		catch { set ::icon.pause.non-active.image $directory/pause-off.png }

		set ::icon.turbo.xcoord [x_coord turbo]
		set ::icon.turbo.ycoord [y_coord turbo]
		set ::icon.turbo.active.fade-delay $load_icons::fade_delay
		set ::icon.turbo.non-active.fade-delay $load_icons::fade_delay
		set ::icon.turbo.active.fade-duration $load_icons::fade_duration
		set ::icon.turbo.non-active.fade-duration $load_icons::fade_duration
		catch { set ::icon.turbo.active.image $directory/turbo-on.png }
		catch { set ::icon.turbo.non-active.image $directory/turbo-off.png }

		set ::icon.fdd.xcoord [x_coord fdd]
		set ::icon.fdd.ycoord [y_coord fdd]
		set ::icon.fdd.active.fade-delay $load_icons::fade_delay
		set ::icon.fdd.non-active.fade-delay $load_icons::fade_delay
		set ::icon.fdd.active.fade-duration $load_icons::fade_duration
		set ::icon.fdd.non-active.fade-duration $load_icons::fade_duration
		catch { set ::icon.fdd.active.image $directory/fdd-on.png }
		catch { set ::icon.fdd.non-active.image $directory/fdd-off.png }

		return ""
	}
}

proc load_icons { set_name } {
	load_icons::load_icons $set_name
}
