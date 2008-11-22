proc show_osd {} {
	set result ""
	foreach widget [osd info] {
		append result "$widget\n"
		foreach property [osd info $widget] {
			append result "  $property [osd info $widget $property]\n"
		}
	}
	return $result
}

proc toggle_fps {} {
	if [info exists ::__osd_fps_after] {
		after cancel $::__osd_fps_after
		osd destroy $::__osd_fps_bg
		unset ::__osd_fps_after ::__osd_fps_bg ::__osd_fps_txt
	} else {
		set ::__osd_fps_bg  [osd create rectangle fps -x 5 -y 5 -z 0 -w 63 -h 20 -rgba 0x00000080]
		set ::__osd_fps_txt [osd create text fps.text -x 5 -y 3 -z 1 -font skins/Vera.ttf.gz -rgba 0xffffffff]
		proc __osd_fps_refresh {} {
			set fps [format "%2.1f" [openmsx_info fps]]
			osd configure $::__osd_fps_txt -text "${fps}FPS"
			set ::__osd_fps_after [after time .2 __osd_fps_refresh]
		}
		__osd_fps_refresh
	}
	return ""
}
