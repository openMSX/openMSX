namespace eval vgm {
variable active false

variable psg_register
variable opll_register
variable y8950_register
variable opl4_register_wave
variable opl4_register_1
variable opl4_register_2

variable start_time
variable ticks
variable music_data
variable file_name
variable original_filename
variable directory [file normalize $::env(OPENMSX_USER_DATA)/../vgm_recordings]

variable psg_logged       true
variable fm_logged        true
variable y8950_logged     false
variable moonsound_logged false
variable scc_logged       false

variable scc_plus_used

variable watchpoints [list]

variable active_fm_register -1

variable supported_chips [list MSX-Music PSG Moonsound MSX-Audio SCC]

proc little_endian_32 {value} {
	binary format i $value
}

proc zeros {value} {
	string repeat "\0" $value
}

set_tabcompletion_proc vgm_rec [namespace code tab_vgmrec]

proc tab_vgmrec {args} {
	variable supported_chips
	concat $supported_chips -prefix
}

proc set_next_filename {} {
	variable original_filename
	variable directory
	variable file_name [utils::get_next_numbered_filename $directory $original_filename ".vgm"]
}

proc vgm_rec_set_filename {filename} {
	variable original_filename

	if {[string last ".vgm" $filename] == -1} {
		set original_filename $filename
	} else {
		set original_filename [string trim $filename ".vgm"]
	}
	set_next_filename
}

vgm_rec_set_filename "music"

set_help_text vgm_rec \
"Starts recording VGM data. Run this before sound chip initialisation,
otherwise it won't work. Supported soundchips: AY8910 (PSG), YM2413 (FMPAC,
MSX-Music), Y8950 (Music Module, MSX-Audio), YMF278B (OPL4, Moonsound) and
Konami SCC(+).
Files will be stored in the openMSX home directory in a subdirectory vgm_recordings
Optional parameters (use tab completion): vgm_rec [tab_vgmrec]
Defaults: Record to music0001.vgm or music0002.vgm if that exists etc.
You may specify a -prefix parameter to change the music file name prefix to
something else.
You must end any recording with vgm_rec_end, otherwise the file will be empty.
Look at vgm_rec_next too.
Additional information:
https://github.com/niekvlessert/openmsx_tcl_vgm_export/blob/master/README.md
"

proc vgm_rec {args} {
	variable psg_logged       false
	variable fm_logged        false
	variable y8950_logged     false
	variable moonsound_logged false
	variable scc_logged       false

	set prefix_index [lsearch -exact $args "-prefix"]
	if {$prefix_index >= 0 && $prefix_index < ([llength $args] - 1)} {
		set prefix [lindex $args [expr {$prefix_index + 1}]]
		set args [lreplace $args $prefix_index [expr {$prefix_index + 1}]]
		vgm_rec_set_filename $prefix
	}

	if {[llength $args] == 0} {
		variable supported_chips
		error "Please specify one or more music chips you want to record VGM data from:\nvgm_rec \[-prefix filename_prefix\] $supported_chips"
	}

	foreach a $args {
		if     {[string compare -nocase $a "PSG"      ] == 0} {set psg_logged       true} \
		elseif {[string compare -nocase $a "MSX-Music"] == 0} {set fm_logged        true} \
		elseif {[string compare -nocase $a "MSX-Audio"] == 0} {set y8950_logged     true} \
		elseif {[string compare -nocase $a "Moonsound"] == 0} {set moonsound_logged true} \
		elseif {[string compare -nocase $a "SCC"      ] == 0} {set scc_logged       true} \
		else {error "Unrecognized argument: $a"}
	}

	vgm_rec_start
}


proc vgm_rec_start {} {
	variable active
	if {$active} {
		error "Already recording."
	}
	set active true

	set_next_filename
	variable directory
	file mkdir $directory

	variable psg_register       -1
	variable fm_register        -1
	variable y8950_register     -1
	variable opl4_register_wave -1
	variable opl4_register_1    -1
	variable opl4_register_2    -1

	variable start_time [machine_info time]
	variable ticks 0
	variable music_data ""

	variable scc_plus_used false

	variable watchpoints
	variable psg_logged
	if {$psg_logged} {
		lappend watchpoints [debug set_watchpoint write_io 0xA0 {} {vgm::write_psg_address}]
		lappend watchpoints [debug set_watchpoint write_io 0xA1 {} {vgm::write_psg_data}]
	}

	variable fm_logged
	if {$fm_logged} {
		lappend watchpoints [debug set_watchpoint write_io 0x7C {} {vgm::write_opll_address}]
		lappend watchpoints [debug set_watchpoint write_io 0x7D {} {vgm::write_opll_data}]
	}

	variable y8950_logged
	if {$y8950_logged} {
		lappend watchpoints [debug set_watchpoint write_io 0xC0 {} {vgm::write_y8950_address}]
		lappend watchpoints [debug set_watchpoint write_io 0xC1 {} {vgm::write_y8950_data}]

		set y8950_ram [concat [machine_info output_port 0xC0] RAM]
		if {[lsearch -exact [debug list] $y8950_ram] >= 0} {
			set y8950_ram_size [debug size $y8950_ram]
			if {$y8950_ram_size > 0} {
				append music_data [binary format ccc 0x67 0x66 0x88]
				append music_data [little_endian_32 [expr $y8950_ram_size + 8]]
				append music_data [little_endian_32 $y8950_ram_size]
				append music_data [zeros 4]
				append music_data [debug read_block $y8950_ram 0 $y8950_ram_size]
			}
		}
	}

	# A thing: for wave to work some bits have to be set through FM2. So
	# that must be logged. This logs all, but just so you know...
	# Another thing: FM data can be used by FM bank 1 and FM bank 2. FM
	# data has a mirror however
	# So programs can use both ports in different ways; all to FM data,
	# FM1->FM-data,FM2->FM-data-mirror, etc. 4 options.
	# http://www.msxarchive.nl/pub/msx/docs/programming/opl4tech.txt
	variable moonsound_logged
	if {$moonsound_logged} {
		lappend watchpoints [debug set_watchpoint write_io 0x7E {} {vgm::write_opl4_address_wave}]
		lappend watchpoints [debug set_watchpoint write_io 0x7F {} {vgm::write_opl4_data_wave}]
		lappend watchpoints [debug set_watchpoint write_io 0xC4 {} {vgm::write_opl4_address_1}]
		lappend watchpoints [debug set_watchpoint write_io 0xC5 {} {vgm::write_opl4_data}]
		lappend watchpoints [debug set_watchpoint write_io 0xC6 {} {vgm::write_opl4_address_2}]
		lappend watchpoints [debug set_watchpoint write_io 0xC7 {} {vgm::write_opl4_data}]

		set moonsound_ram [concat [machine_info output_port 0x7E] {wave RAM}]
		if {[lsearch -exact [debug list] $moonsound_ram] >= 0} {
			set moonsound_ram_size [debug size $moonsound_ram]
			if {$moonsound_ram_size > 0} {
				append music_data [binary format ccc 0x67 0x66 0x87]
				append music_data [little_endian_32 [expr $moonsound_ram_size + 8]]
				append music_data [little_endian_32 $moonsound_ram_size]
				append music_data [zeros 4]
				append music_data [debug read_block $moonsound_ram 0 $moonsound_ram_size]
			}
		}
	}

	variable scc_logged
	if {$scc_logged} {
		foreach {ps ss plus} [find_all_scc] {
			if {$plus} {
				lappend watchpoints [debug set_watchpoint write_mem {0xB800 0xB8AF} "\[watch_in_slot $ps $ss\]" {vgm::scc_plus_data}]
			} else {
				lappend watchpoints [debug set_watchpoint write_mem {0x9800 0x988F} "\[watch_in_slot $ps $ss\]" {vgm::scc_data}]
			}
		}
	}

	variable file_name
	set recording_text "VGM recording started to $file_name.\nRecording data for the following sound chips:"
	if {$psg_logged      } {append recording_text " PSG"       }
	if {$fm_logged       } {append recording_text " MSX-Music" }
	if {$y8950_logged    } {append recording_text " MSX-Audio" }
	if {$moonsound_logged} {append recording_text " Moondsound"}
	if {$scc_logged      } {append recording_text " SCC"       }
	puts $recording_text
	message $recording_text
}

proc find_all_scc {} {
	set result [list]
	for {set ps 0} {$ps < 4} {incr ps} {
		for {set ss 0} {$ss < 4} {incr ss} {
			set device_list [machine_info slot $ps $ss 2]
			if {[llength $device_list] != 0} {
				set device [lindex $device_list 0]
				set device_info_list [machine_info device $device]
				lassign $device_info_list device_info device_sub_info
				if {[string match -nocase *scc* $device_info]} {
					lappend result $ps $ss 1
				}
				if {[string match -nocase *scc* $device_sub_info] ||
				    [string match -nocase manbow2 $device_sub_info] ||
				    [string match -nocase KonamiUltimateCollection $device_sub_info]} {
					lappend result $ps $ss 0
				}
			}
			if {![machine_info issubslotted $ps]} break
		}
	}
	return $result
}

proc write_psg_address {} {
	variable psg_register $::wp_last_value
}

proc write_psg_data {} {
	variable psg_register
	if {$psg_register >= 0 && $psg_register < 14} {
		update_time
		variable music_data
		append music_data [binary format ccc 0xA0 $psg_register $::wp_last_value]
	}
}

proc write_opll_address {} {
	variable opll_register $::wp_last_value
}

proc write_opll_data {} {
	variable opll_register
	if {$opll_register >= 0} {
		update_time
		variable music_data
		append music_data [binary format ccc 0x51 $opll_register $::wp_last_value]
	}
}

proc write_y8950_address {} {
	variable y8950_register $::wp_last_value
}

proc write_y8950_data {} {
	variable y8950_register
	if {$y8950_register >= 0} {
		update_time
		variable music_data
		append music_data [binary format ccc 0x5C $y8950_register $::wp_last_value]
	}
}

proc write_opl4_address_wave {} {
	variable opl4_register_wave $::wp_last_value
}

proc write_opl4_data_wave {} {
	variable opl4_register_wave
	if {$opl4_register_wave >= 0} {
		update_time
		# VGM spec: Port 0 = FM1, port 1 = FM2, port 2 = Wave. It's
		# based on the datasheet A1 & A2 use.
		variable music_data
		append music_data [binary format cccc 0xD0 0x2 $opl4_register_wave $::wp_last_value]
	}
}

proc write_opl4_address_1 {} {
	variable opl4_register_1 $::wp_last_value
	variable active_fm_register 1
}

proc write_opl4_data {} {
	variable opl4_register_1
	variable opl4_register_2
	variable active_fm_register
	variable music_data

	if {($opl4_register_1 >= 0 && $active_fm_register == 1)} {
		update_time
		append music_data [binary format cccc 0xD0 0x0 $opl4_register_1 $::wp_last_value]
	}
	if {($opl4_register_2 >= 0 && $active_fm_register == 2)} {
		update_time
		append music_data [binary format cccc 0xD0 0x1 $opl4_register_2 $::wp_last_value]
	}
}

proc write_opl4_address_2 {} {
	variable opl4_register_2 $::wp_last_value
	variable active_fm_register 2
}

proc scc_data {} {
	# Thanks ValleyBell, BiFi

	# if 9800h is written, waveform channel 1   is set in 9800h - 981fh, 32 bytes
	# if 9820h is written, waveform channel 2   is set in 9820h - 983fh, 32 bytes
	# if 9840h is written, waveform channel 3   is set in 9840h - 985fh, 32 bytes
	# if 9860h is written, waveform channel 4,5 is set in 9860h - 987fh, 32 bytes
	# if 9880h is written, frequency channel 1 is set in 9880h - 9881h, 12 bits
	# if 9882h is written, frequency channel 2 is set in 9882h - 9883h, 12 bits
	# if 9884h is written, frequency channel 3 is set in 9884h - 9885h, 12 bits
	# if 9886h is written, frequency channel 4 is set in 9886h - 9887h, 12 bits
	# if 9888h is written, frequency channel 5 is set in 9888h - 9889h, 12 bits
	# if 988ah is written, volume channel 1 is set, 4 bits
	# if 988bh is written, volume channel 2 is set, 4 bits
	# if 988ch is written, volume channel 3 is set, 4 bits
	# if 988dh is written, volume channel 4 is set, 4 bits
	# if 988eh is written, volume channel 5 is set, 4 bits
	# if 988fh is written, channels 1-5 on/off, 1 bit

	#VGM port format:
	#0x00 - waveform
	#0x01 - frequency
	#0x02 - volume
	#0x03 - key on/off
	#0x04 - waveform (0x00 used to do SCC access, 0x04 SCC+)
	#0x05 - test register

	update_time

	variable music_data
	if {0x9800 <= $::wp_last_address && $::wp_last_address < 0x9880} {
		append music_data [binary format cccc 0xD2 0x0 [expr {$::wp_last_address - 0x9800}] $::wp_last_value]
	}
	if {0x9880 <= $::wp_last_address && $::wp_last_address < 0x988A} {
		append music_data [binary format cccc 0xD2 0x1 [expr {$::wp_last_address - 0x9880}] $::wp_last_value]
	}
	if {0x988A <= $::wp_last_address && $::wp_last_address < 0x988F} {
		append music_data [binary format cccc 0xD2 0x2 [expr {$::wp_last_address - 0x988A}] $::wp_last_value]
	}
	if {$::wp_last_address == 0x988F} {
		append music_data [binary format cccc 0xD2 0x3 0x0 $::wp_last_value]
	}
}

proc scc_plus_data {} {
	# if b800h is written, waveform channel 1 is set in b800h - b81fh, 32 bytes
	# if b820h is written, waveform channel 2 is set in b820h - b83fh, 32 bytes
	# if b840h is written, waveform channel 3 is set in b840h - b85fh, 32 bytes
	# if b860h is written, waveform channel 4 is set in b860h - b87fh, 32 bytes
	# if b880h is written, waveform channel 5 is set in b880h - b89fh, 32 bytes
	# if b8a0h is written, frequency channel 1 is set in b8a0h - b8a1h, 12 bits
	# if b8a2h is written, frequency channel 2 is set in b8a2h - b8a3h, 12 bits
	# if b8a4h is written, frequency channel 3 is set in b8a4h - b8a5h, 12 bits
	# if b8a6h is written, frequency channel 4 is set in b8a6h - b8a7h, 12 bits
	# if b8a8h is written, frequency channel 5 is set in b8a8h - b8a9h, 12 bits
	# if b8aah is written, volume channel 1 is set, 4 bits
	# if b8abh is written, volume channel 2 is set, 4 bits
	# if b8ach is written, volume channel 3 is set, 4 bits
	# if b8adh is written, volume channel 4 is set, 4 bits
	# if b8aeh is written, volume channel 5 is set, 4 bits
	# if b8afh is written, channels 1-5 on/off, 1 bit

	#VGM port format:
	#0x00 - waveform
	#0x01 - frequency
	#0x02 - volume
	#0x03 - key on/off
	#0x04 - waveform (0x00 used to do SCC access, 0x04 SCC+)
	#0x05 - test register

	update_time

	variable music_data
	if {0xB800 <= $::wp_last_address && $::wp_last_address < 0xB8A0} {
		append music_data [binary format cccc 0xD2 0x4 [expr {$::wp_last_address - 0xB800}] $::wp_last_value]
	}
	if {0xB8A0 <= $::wp_last_address && $::wp_last_address < 0xb8aa} {
		append music_data [binary format cccc 0xD2 0x1 [expr {$::wp_last_address - 0xB8A0}] $::wp_last_value]
	}
	if {0xB8AA <= $::wp_last_address && $::wp_last_address < 0xB8AF} {
		append music_data [binary format cccc 0xD2 0x2 [expr {$::wp_last_address - 0xB8AA}] $::wp_last_value]
	}
	if {$::wp_last_address == 0xB8AF} {
		append music_data [binary format cccc 0xD2 0x3 0x0 $::wp_last_value]
	}

	variable scc_plus_used true
}

proc update_time {} {
	variable start_time
	set new_ticks [expr {int(([machine_info time] - $start_time) * 44100)}]

	variable ticks
	variable music_data
	while {$new_ticks > $ticks} {
		set difference [expr {$new_ticks - $ticks}]
		set step [expr {$difference > 65535 ? 65535 : $difference}]
		incr ticks $step
		append music_data [binary format cs 0x61 $step]
	}
}

set_help_text vgm_rec_end \
{Ends recording VGM data; writes VGM header and data to disk.
Look at vgm_rec and vgm_rec_next too.
}

proc vgm_rec_end {} {
	variable active
	if {!$active} {
		error "Not recording."
	}

	# remove all watchpoints that were created
	variable watchpoints
	foreach watch $watchpoints {
		debug remove_watchpoint $watch
	}
	variable watchpoints [list]

	update_time
	variable music_data
	append music_data [binary format c 0x66]

	set header "Vgm "
	# file size
	append header [little_endian_32 [expr {[string length $music_data] + 0x100 - 4}]]
	# VGM version 1.7
	append header [little_endian_32 0x170]
	append header [zeros 4]

	# YM2413 clock
	variable fm_logged
	if {$fm_logged} {
		append header [little_endian_32 3579545]
	} else {
		append header [zeros 4]
	}

	append header [zeros 4]
	# Number of ticks
	variable ticks
	append header [little_endian_32 $ticks]
	append header [zeros 24]
	# Data starts at offset 0x100
	append header [little_endian_32 [expr {0x100 - 0x34}]]
	append header [zeros 32]

	# Y8950 clock
	variable y8950_logged
	if {$y8950_logged} {
		append header [little_endian_32 3579545]
	} else {
		append header [zeros 4]
	}

	append header [zeros 4]

	# YMF278B clock
	variable moonsound_logged
	if {$moonsound_logged} {
		append header [little_endian_32 33868800]
	} else {
		append header [zeros 4]
	}

	append header [zeros 16]

	# AY8910 clock
	variable psg_logged
	if {$psg_logged} {
		append header [little_endian_32 1789773]
	} else {
		append header [zeros 4]
	}

	append header [zeros 36]

	# SCC clock
	variable scc_logged
	if {$scc_logged} {
		set scc_clock 1789773
		variable scc_plus_used
		if {$scc_plus_used} {
			# enable bit 31 for SCC+ support, that's how it's done
			# in VGM I've been told. Thanks Grauw.
			set scc_clock [expr {$scc_clock | 1 << 31}]
		}
		append header [little_endian_32 $scc_clock]
	} else {
		append header [zeros 4]
	}

	append header [zeros 96]

	variable file_name
	set file_handle [open $file_name "w"]
	fconfigure $file_handle -encoding binary -translation binary
	puts -nonewline $file_handle $header
	puts -nonewline $file_handle $music_data
	close $file_handle

	set active false

	set stop_message "VGM recording stopped, wrote data to $file_name."
	puts $stop_message
	message $stop_message
}

set_help_text vgm_rec_next \
{This will end the previous recording and start the next one with the same
sound chip parameters and filename with an increased number in the filename.
With this you can easily put multiple songs in separate files so you don't have
to split them afterwards.
Be careful; this function won't work always; the second and beyond file might
not contain any soundchip initialisation stuff.
For SCC it works fine, because no sound chip initialisation is required, but
for Moonsound it might not because of this, if the player engine is not doing
all initialisation with every track. This can be fixed as well, but that'll
require more work, better use the vgm_tools for splitting those.
It's useful to bind this function to a key, to easily skip to the next file. On
Mac for example: 'bind N+META vgm_rec_next', then Cmd-N will skip to the next
track.
}
proc vgm_rec_next {} {
	variable active
	if {!$active} {
		variable original_filename music
	} else {
		vgm_rec_end
	}
	vgm_rec_start
}

namespace export vgm_rec
namespace export vgm_rec_next
namespace export vgm_rec_end
}

namespace import vgm::*
