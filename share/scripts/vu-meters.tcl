# VU-meters, just for fun.
#
# Search in the script for 'customized' to see what you could customize ;-)
#
# The volume calculations are the volume setting of the channel, unless nothing
# is played, then 0 is output. This is done by looking at 'key on' bits, as far
# as they exist.
#
# TODO:
# - optimize more?
# - implement volume for MoonSound FM (tricky stuff)
# - actually, don't use regs to calc volume but actual wave data (needs openMSX
#   changes)
# - handle insertion/removal of sound devices (needs openMSX changes)
#
# Thanks to BiFi for helping out with the expressions for the devices.
# Thanks to Wouter for the Tcl support.

set_help_text toggle_vu_meters \
{Puts a rough volume unit display for each channel and for all sound chips on
the On-Screen-Display. Use the command again to remove it. Note: it cannot
handle run-time insertion/removal of sound devices. It can handle changing
of machines, though. Not all chips are supported yet.
Note that displaying these VU-meters may cause quite some CPU load!}

namespace eval vu_meters {

variable vu_meters_active false
variable volume_cache
variable volume_expr
variable nof_channels
variable bar_length
variable soundchips
variable machine_switch_trigger_id
variable frame_trigger_id

proc get_num_channels {soundchip} {
	set num 1
	while {[info exists ::${soundchip}_ch${num}_mute]} {
		incr num
	}
	expr $num - 1
}

proc vu_meters_init {} {

	variable volume_cache
	variable volume_expr
	variable nof_channels
	variable bar_length
	variable soundchips
	variable machine_switch_trigger_id

	# create root object for vu_meters
	osd create rectangle vu_meters \
		-scaled true \
		-alpha 0 \
		-z 1

	set soundchips [list]

	foreach soundchip [machine_info sounddevice] {
		# determine number of channels
		set channel_count [get_num_channels $soundchip]
		# skip devices which don't have volume expressions (not implemented yet)
		if {[get_volume_expr_for_channel $soundchip 0] == "x"} continue
			
		lappend soundchips $soundchip
		set nof_channels($soundchip) $channel_count
		for {set i 0} {$i < $channel_count} {incr i} {
			# create the volume cache and the expressions
			set volume_cache($soundchip,$i) -1
			set volume_expr($soundchip,$i) [get_volume_expr_for_channel $soundchip $i]
		}
	}

#	puts stderr [parray volume_expr]; # debug

	set bar_width 2; # this value could be customized
	set vu_meter_title_height 8; # this value could be customized
	set bar_length [expr (320 - [llength $soundchips]) / [llength $soundchips]]
	if {$bar_length > (320/4)} {set bar_length [expr 320/4]}

	# create widgets for each sound chip:

	set vu_meter_offset 0

	foreach soundchip $soundchips {

		# create surrounding widget for this chip
		osd create rectangle vu_meters.$soundchip \
			-rgba 0x00000080 \
			-x ${vu_meter_offset} \
			-y 0 \
			-w $bar_length \
			-h [expr $vu_meter_title_height + 1 + $nof_channels($soundchip) * ($bar_width + 1)] \
			-clip true
		osd create text vu_meters.${soundchip}.title \
			-x 1 \
			-y 1 \
			-rgba 0xffffffff \
			-text $soundchip \
			-size [expr $vu_meter_title_height - 1]

		# create vu meters for this sound chip
		for {set channel 0} {$channel < $nof_channels($soundchip)}  {incr channel} {
				osd create rectangle vu_meters.${soundchip}.ch${channel} \
				-rgba 0xff0000ff \
				-x 0 \
				-y [expr $vu_meter_title_height + 1 + (($bar_width + 1) * $channel)] \
				-w 0 \
				-h $bar_width \
		}

		incr vu_meter_offset [expr $bar_length + 1]
	}

	set machine_switch_trigger_id [after machine_switch [namespace code vu_meters_reset]]
}

proc update_meters {} {
	variable vu_meters_active
	variable volume_cache
	variable volume_expr
	variable nof_channels
	variable soundchips
	variable frame_trigger_id

	# update meters with the volumes
	if {!$vu_meters_active} return

	foreach soundchip $soundchips {
		for {set channel 0} {$channel < $nof_channels($soundchip)}  {incr channel} {
			set new_volume [eval $volume_expr($soundchip,$channel)]
			if {$volume_cache($soundchip,$channel) != $new_volume} {
				update_meter "vu_meters.${soundchip}.ch${channel}" $new_volume
				set volume_cache($soundchip,$channel) $new_volume
			}
		}
	}
	# here you can customize the update frequency (to reduce CPU load)
	#set frame_trigger_id [after time 0.05 [namespace code update_meters]]
	set frame_trigger_id [after frame [namespace code update_meters]]
}

proc update_meter {meter volume} {
	variable bar_length

	set byte_val [expr {round(255 * $volume)}]
	osd configure $meter \
		-w [expr {$bar_length * $volume}] \
		-rgba [expr {($byte_val << 24) + ((255 ^ $byte_val) << 8) + 0x008000C0}]
}

proc get_volume_expr_for_channel {soundchip channel} {
	# note: channel number starts with 0 here
	switch [machine_info sounddevice $soundchip] {
		"PSG" {
			return "set keybits \[debug read \"${soundchip} regs\" 7\]; expr ((\[debug read \"${soundchip} regs\" [expr $channel + 8]\] &0xF)) / 15.0 * !((\$keybits >> $channel) & (\$keybits >> [expr $channel + 3]) & 1)"
		}
		"MoonSound wave-part" {
			return "expr (127 - (\[debug read \"${soundchip} regs\" [expr $channel + 0x50]\] >> 1)) / 127.0 * \[expr \[debug read \"${soundchip} regs\" [expr $channel + 0x68]\] >> 7\]";
		}
		"Konami SCC" -
		"Konami SCC+" {
			return "expr ((\[debug read \"${soundchip} SCC\" [expr $channel + 0xAA]\] &0xF)) / 15.0 * \[expr ((\[debug read \"${soundchip} SCC\" 0xAF\] >> $channel) &1)\]"
		}
		"MSX-MUSIC" {
			return "expr (15 - (\[debug read \"${soundchip} regs\" [expr $channel + 0x30]\] &0xF)) / 15.0 * \[expr ((\[debug read \"${soundchip} regs\" [expr $channel + 0x20]] &16)) >> 4\]";# carrier total level, but only when key bit is on for this channel
		}
		"MSX-AUDIO" {
			if {$channel == 11} { ;# ADPCM
				return "expr \[debug read \"${soundchip} regs\" 0x12\] / 255.0";# can we output 0 when no sample is playing?
			} else {
				set offset $channel
				if {$channel > 2} {
					incr offset 5
				}
				if {$channel > 5} {
					incr offset 5
				}
				return "expr (63 - (\[debug read \"${soundchip} regs\" [expr $offset + 0x43]\] &63)) / 63.0 * \[expr ((\[debug read \"${soundchip} regs\" [expr $channel + 0xB0]] &32)) >> 5\] "
			}
		}
		default {
			return "x"
		}
	}
}

proc vu_meters_reset {} {
	variable vu_meters_active
	if {!$vu_meters_active} {
		error "Please fix a bug in this script!"
	}
	toggle_vu_meters
	toggle_vu_meters
}

proc toggle_vu_meters {} {
	variable vu_meters_active
	variable machine_switch_trigger_id
	variable frame_trigger_id
	variable soundchips
	variable bar_length
	variable volume_cache
	variable volume_expr
	variable nof_channels

	if {$vu_meters_active} {
		catch {after cancel $machine_switch_trigger_id}
		catch {after cancel $frame_trigger_id}
		set vu_meters_active false
		osd destroy vu_meters
		unset soundchips bar_length volume_cache volume_expr nof_channels
	} else {
		set vu_meters_active true
		vu_meters_init
		update_meters
	}
}

#proc parray {name} {
#	upvar $name local
#	set result ""
#	foreach key [array names local] {
#		append result "${name}(${key}) = $local($key)\n"
#	}
#	return $result
#}

namespace export toggle_vu_meters

} ;# namespace vu_meters

namespace import vu_meters::*
