# several utility procs for usage in other scripts
# don't export anything, just use it from the namespace,
# because these scripts aren't useful for console users
# and should therefore not be exprted to the global
# namespace.
#
# these procs are specific for sound chips
#
# Born to prevent duplication between scripts for common stuff.

namespace eval soundchip_utils {

proc get_num_channels {soundchip} {
	set num 1
	while {[info exists ::${soundchip}_ch${num}_mute]} {
		incr num
	}
	expr $num - 1
}

#
# It is advised to cache the result of this proc for each channel of each sound device
# before using it, because then a lot will be pre-evaluated and at run time only the
# actual variable stuff will be evaluated.
# We cannot cache it here, because we don't know the names of the sound chips in the
# machine you are going to use this on.
# @param soundchip the name of the soundchip as it appears in the output of
# "machine_info sounddevice"
# @channel the channel for which you want the expression to get the volume, the
# first channel is channel 0 and the channels are the ones as they are output
# by the record_channels command
# @return expression to calculate the volume of the device for that channel in
# range [0-1]; returns just 'x' in case the chip is not supported
#
proc get_volume_expr {soundchip channel} {
	# note: channel number starts with 0 here
	switch [machine_info sounddevice $soundchip] {
		"PSG" {
			return "set keybits \[debug read \"${soundchip} regs\" 7\]; expr ((\[debug read \"${soundchip} regs\" [expr $channel + 8]\] &0xF)) / 15.0 * !((\$keybits >> $channel) & (\$keybits >> [expr $channel + 3]) & 1)"
		}
		"MoonSound wave-part" {
			return "expr (\[debug read \"${soundchip} regs\" [expr $channel + 0x68]\]) ? (127 - (\[debug read \"${soundchip} regs\" [expr $channel + 0x50]\] >> 1)) / 127.0 : 0.0";
		}
		"Konami SCC" -
		"Konami SCC+" {
			return "expr ((\[debug read \"${soundchip} SCC\" [expr $channel + 0xAA]\] &0xF)) / 15.0 * ((\[debug read \"${soundchip} SCC\" 0xAF\] >> $channel) &1)"
		}
		"MSX-MUSIC" {
			set music_mode_expr "(((\[debug read \"${soundchip} regs\" [expr $channel + 0x20]] &16)) ? ((15 - (\[debug read \"${soundchip} regs\" [expr $channel + 0x30]\] &0xF)) / 15.0) : 0.0)";# carrier total level, but only when key bit is on for this channel
			if {$channel < 6} {
				return "expr $music_mode_expr"
			} else {
				if { $channel < 9 } {
					set vol_expr "(\[debug read \"${soundchip} regs\" [expr $channel + 0x30]\] & 15)"
				} else {
					set vol_expr "(\[debug read \"${soundchip} regs\" [expr $channel + 0x2E]\] >> 4)"
				}
				switch $channel {
					6  { set onmask 16 } ;# BD
					7  { set onmask 8  } ;# SD
					8  { set onmask 2  } ;# T-CYM
					9  { set onmask 1  } ;# HH
					10 { set onmask 4  } ;# TOM
					default {
						error "Unknown channel: $channel for $soundchip!"
					}
				}
				return "set rhythm \[debug read \"${soundchip} regs\" 0x0E\]; expr (\$rhythm & 32) ? ((\$rhythm & $onmask) ? (15 - $vol_expr) / 15.0 : 0.0) : $music_mode_expr"
			}
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
				set music_mode_expr "(((\[debug read \"${soundchip} regs\" [expr $channel + 0xB0]] &32)) ? (63 - (\[debug read \"${soundchip} regs\" [expr $offset + 0x43]\] & 63)) / 63.0 : 0.0)"
				if {$channel < 6} {
					return "expr $music_mode_expr"
				} else {
					switch $channel {
						6  { set onmask 16; set offset 0x10 } ;# BD (slot 16)
						7  { set onmask 8;  set offset 0x11 } ;# SD (slot 17)
						8  { set onmask 2;  set offset 0x12 } ;# T-CYM (slot 18)
						9  { set onmask 1;  set offset 0x0E } ;# HH (slot 13)
						10 { set onmask 4;  set offset 0x0F } ;# TOM (slot 14)
						default {
							error "Unknown channel: $channel for $soundchip!"
						}
					}
					return "set rhythm \[debug read \"${soundchip} regs\" 0xBD\]; expr (\$rhythm & 32) ? ((\$rhythm & $onmask) ? ((63 - (\[debug read \"${soundchip} regs\" [expr $offset + 0x43]\] & 63)) / 63.0) : 0.0) : $music_mode_expr"
				}
			}
		}
		default {
			return "x"
		}
	}
}

} ;# namespace soundchip_utils
