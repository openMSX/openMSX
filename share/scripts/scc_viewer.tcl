# thanks to bifimsx for his help and his technical documentation @
# http://bifi.msxnet.org/msxnet/tech/scc.html
#
# TODO: 
# - optimize! (A LOT!)
# - support SCC-I

set_help_text toggle_scc_viewer\
{Toggles display of the SCC viewer in which you can follow the wave forms and
volume per SCC channel in real time. Note: it doesn't explicitly support SCC-I
yet and it can take up quite some CPU load...}

namespace eval scc_viewer {

variable scc_viewer_active false
variable scc_devices
variable num_samples 32
variable num_channels 5
variable vertical_downscale_factor 4
variable channel_height [expr 256 / $vertical_downscale_factor]
variable machine_switch_trigger_id
variable frame_trigger_id

set volume_address [expr $num_samples * $num_channels + 2 * $num_channels]

proc scc_viewer_init {} {
	variable machine_switch_trigger_id
	variable scc_viewer_active
	variable scc_devices
	variable num_channels
	variable num_samples
	variable vertical_downscale_factor
	variable channel_height

	set scc_devices [list]

	foreach soundchip [machine_info sounddevice] {
		switch [machine_info sounddevice $soundchip] {
			"Konami SCC" -
			"Konami SCC+" {
				lappend scc_devices $soundchip
			}
		}
	}

	#set base element	
	osd create rectangle scc_viewer \
		-x 2 \
		-y 2 \
		-alpha 0

	set textheight 15
	set border_width 2
	set inter_channel_spacing 8
	set device_width [expr $num_channels * ($num_samples + $inter_channel_spacing) \
		- $inter_channel_spacing + 2 * $border_width]

	#create channels
	set number 0
	set offset 0
	foreach device $scc_devices {
		osd create rectangle scc_viewer.$device \
			-x [expr $offset + $number * $device_width] \
			-y 0 \
			-h [expr $channel_height + 2 * $border_width + $textheight] \
			-w $device_width \
			-rgba 0xffffff20 \
			-clip true
		osd create text scc_viewer.$device.title \
			-x 0 \
			-y 0 \
			-rgba 0xffffffff \
			-text $device \
			-size $textheight
		for {set chan 0} {$chan < $num_channels} {incr chan} {
			osd create rectangle scc_viewer.$device.$chan \
				-x [expr ($chan * ($num_samples + $inter_channel_spacing)) + $border_width] \
				-y [expr $border_width + $textheight] \
				-h $channel_height \
				-w $num_samples \
				-z 0 \
				-rgba 0x0000ff80
			osd create rectangle scc_viewer.$device.$chan.volume \
				-x 0 \
				-y 0 \
				-h 0 \
				-w $num_samples \
				-z 2 \
				-rgba 0x0077ff80
			osd create rectangle scc_viewer.$device.$chan.mid \
				-x 0 \
				-y [expr $channel_height / 2 - 1] \
				-h 1 \
				-w $num_samples \
				-z 32 \
				-rgba 0xddddddff
			for {set pos 0} {$pos < $num_samples} {incr pos} {
				osd create rectangle scc_viewer.$device.$chan.$pos \
					-x $pos \
					-y [expr $channel_height / 2] \
					-h 0 \
					-w 1 \
					-z 16 \
					-rgba 0xffffffb0
			}
		}
		incr number
		set offset 10
	}
	set machine_switch_trigger_id [after machine_switch [namespace code scc_viewer_reset]]	
}

proc update_scc_viewer {} {
	variable scc_viewer_active
	variable scc_devices
	variable num_channels
	variable num_samples
	variable vertical_downscale_factor
	variable channel_height
	variable frame_trigger_id

	if {!$scc_viewer_active} return

	foreach device $scc_devices {
		for {set chan 0} {$chan < $num_channels} {incr chan} {
			for {set pos 0} {$pos < $num_samples} {incr pos} {
				osd configure scc_viewer.$device.$chan.$pos \
					-h [expr { [get_scc_wave [debug read "$device SCC" [expr (($chan * $num_samples) + $pos)]]] / $vertical_downscale_factor}]
			}
			set volume [expr {[debug read "$device SCC" [ expr $scc_viewer::volume_address + $chan]] * 4}]
			osd configure scc_viewer.$device.$chan.volume \
				-h $volume \
				-y [expr {($channel_height - $volume) / 2}]
		}
	}
	# set frame_trigger_id [after frame [namespace code {puts [time update_scc_viewer]}]];# for profiling
	set frame_trigger_id [after frame [namespace code update_scc_viewer]]
}

proc get_scc_wave {sccval} { return [expr $sccval < 128 ? $sccval : $sccval - 256] }

proc scc_viewer_reset {} {
	variable scc_viewer_active
	if {!$scc_viewer_active} {
		error "Please fix a bug in this script!"
	}
	toggle_scc_viewer
	toggle_scc_viewer
}

proc toggle_scc_viewer {} {
	variable scc_viewer_active
	variable machine_switch_trigger_id
	variable frame_trigger_id

	if {$scc_viewer_active} {
		catch {after cancel $machine_switch_trigger_id}
		catch {after cancel $frame_trigger_id}
		set scc_viewer_active false
		osd destroy scc_viewer
	} else {
		set scc_viewer_active true
		scc_viewer_init
		update_scc_viewer
	}
}

namespace export toggle_scc_viewer

} ;# namespace scc_viewer

namespace import scc_viewer::*
