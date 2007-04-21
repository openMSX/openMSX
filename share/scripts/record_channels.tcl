set_help_text record_channels \
{Convenience function to control recording of individual channels of
sounddevice(s).

There are three subcommand: start, stop and list to respectively start
recording additional channels, to stop recording all or some channels and
to list which channels are currently beiing recorded.
  record_channels [start] [<device> [<channels>]]
  record_channels  stop   [<device> [<channels>]]
  record_channels  list

Some examples will make it much clearer:
  - To start recording:
      record_channels start PSG      record all PSG channels
      record_channels PSG            the 'start' keyword can be left out
      record_channels SCC 1,4-5      only record channels 1, 4 and 5
      record_channels SCC PSG 1      record all SCC channels, PSG channel 1
  - To stop recording
      record_channels stop           stop all recording
      record_channels stop PSG       stop recording all PSG channels
      record_channels stop SCC 3,5   stop recording SCC channels 3 and 5
  - To show the current status
      record_channels list           shows which channels are beiing recorded
}

set_tabcompletion_proc record_channels tab_record_channels
proc tab_record_channels { args } {
	set result [machine_info sounddevice]
	if {[llength $args] == 2} {
		set result [join [list $result "start stop list"]]
	}
	return $result
}

proc parse_channel_numbers { str } {
	set result [list]
	foreach a [split $str ", "] {
		set b [split $a "-"]
		foreach c $b {
			if ![string is integer $c] {
				error "Not an integer: $c"
			}
		}
		switch [llength $b] {
			0 { }
			1 { lappend result [lindex $b 0] }
			2 { for {set i [lindex $b 0]} {$i <= [lindex $b 1]} {incr i} {
				lappend result $i
			}}
			default { error "Invalid range: $a" }
		}
	}
	return [lsort -unique $result]
}

proc get_all_channels { device } {
	set i 1
	set channels [list]
	while {[info exists ::${device}_ch${i}_record]} {
		lappend channels $i
		incr i
	}
	return $channels
}

proc get_recording_channels { } {
	set result [list]
	set sounddevices [machine_info sounddevice]
	foreach device $sounddevices {
		set active [list]
		foreach ch [get_all_channels $device] {
			set var ::${device}_ch${ch}_record
			if {[set $var] != ""} {
				lappend active $ch
			}
		}
		if [llength $active] {
			lappend result "$device: $active"
		}
	}
	return $result 
}

proc record_channels { args } {
	set start true
	set device_channels [list]
	set sounddevices [machine_info sounddevice]

	# parse subcommand (default is start)
	set first [lindex $args 0]
	switch $first {
		list {
			return [join [get_recording_channels] "\n"]
		}
		start -
		stop {
			set start [string equal $first "start"]
			set args [lrange $args 1 end]
		}
	}

	# parse devices/channels
	while {[llength $args]} {
		set device [lindex $args 0]
		set args [lrange $args 1 end]
		if {[lsearch $sounddevices $device] == -1} {
			error "Unknown sounddevice: $device"
		}
		set range [lindex $args 0]
		if {($range != "") && ([lsearch $sounddevices $range] == -1)} {
			set channels [parse_channel_numbers $range]
			set args [lrange $args 1 end]
		} else {
			set channels [get_all_channels $device]
		}
		lappend device_channels $device $channels
	}

	# stop without any further arguments -> stop all
	if {!$start && ![llength $device_channels]} {
		foreach device $sounddevices {
			set channels [get_all_channels $device]
			lappend device_channels $device $channels
		}
	}

	# actually start/stop recording
	foreach {device channels} $device_channels {
		foreach ch $channels {
			set var ::${device}_ch${ch}_record
			if ![info exists $var] {
				error "No channel $ch on sounddevice $device"
			}
			if $start {
				set $var ${device}-ch${ch}.wav
			} else {
				set $var ""
			}
		}
	}
}
