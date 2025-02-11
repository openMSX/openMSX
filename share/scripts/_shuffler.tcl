namespace eval shuffler {

namespace ensemble create

set_help_text shuffler \
"Starts MSX instances for each ROM file in the given folder and switches
between these machines after the given switch_time has passed. The next game is
the next game in the folder (for round_robin order) or a random game (for
random order).

Usage:
shuffler start <parameters> - starts the shuffler, with parameters below.
shuffler stop               - stops the shuffler.

Parameters:
folder: the folder to use to look for the ROM files to run
switch_time: the time in seconds after which the shuffler will switch to the next game (default: 10)
order: optional: the order in which the games are switched (default: round_robin, i.e. in fixed order)
machine: optional: the machine to use (default: C-BIOS_MSX2+)
extensions: optional: the extensions to look for (default: {.rom .ri .ROM})"

variable machines_running [list]

variable max_num_files 50

variable interval 0
variable shuffle_order
variable after_id ""
variable original_machine ""

proc start {folder {switch_time 10} {order round_robin} {machine {C-BIOS_MSX2+}} {extensions {.rom .ri .ROM}} } {
	variable max_num_files
	variable interval
	variable shuffle_order
	variable machines_running
	variable after_id
	variable original_machine

	if {[llength $machines_running] > 0} {
		error "Shuffler already running, stop it first..."
	}

	foreach ext $extensions {
		lappend pattern "*$ext"
	}
	set files [glob -directory $folder -nocomplain {*}$pattern]

	set num_files [llength $files]
	if {$num_files > $max_num_files} {
		error "Too many files found in folder $folder with extensions $extensions: $num_files, the maximum is $max_num_files"
	} elseif {$num_files <= 1} {
		error "Not enough suitable files found in $folder matching $extensions... aborting."
	}

	foreach file_to_run $files {
		set id [create_machine]
		if {[catch {${id}::load_machine $machine} error_result]} {
			stop
			error "Error starting machine $machine: $error_result"
		}
		if {[catch {${id}::carta $file_to_run} error_result]} {
			stop
			error "Error starting $file_to_run: $error_result"
		}
		lappend machines_running $id
	}

	set interval $switch_time
	set shuffle_order $order
	set original_machine [activate_machine]
	next_machine
	return "Shuffling with: [join $files {, }]"
}

proc stop {} {
	variable after_id
	variable machines_running
	variable original_machine

	after cancel $after_id
	foreach machine $machines_running {
		delete_machine $machine
	}
	set machines_running [list]
	if {$original_machine ne ""} {
		activate_machine $original_machine
		set original_machine ""
	}
}

proc next_machine {} {
	variable machines_running
	variable interval
	variable shuffle_order
	variable after_id

	set current_machine [activate_machine]
	set index [lsearch -exact $machines_running $current_machine]
	set num_machines [llength $machines_running]
	if {$shuffle_order eq "random"} {
		if {$num_machines > 1} {
			set old_index $index
			while {$index == $old_index} {
				set index [expr {round([utils::get_random_number $num_machines])}]
			}
		}
	} else {
		set index [expr {($index + 1) % $num_machines}]
	}
	activate_machine [lindex $machines_running $index]

	set after_id [after time $interval [namespace code next_machine]]
}

proc tabcompletion {args} {
	if {[lsearch -exact $args "start"] >= 0} {
		switch [llength $args] {
			3 {puts [lindex $args 2]; utils::file_completion [lindex $args 2]}
			4 {}
			5 { concat round_robin random }
			6 { openmsx_info machines }
			default {}
		}
	} else {
		concat start stop
	}
}

set_tabcompletion_proc shuffler [namespace code tabcompletion]

namespace export start stop

} ;# namespace shuffler

namespace import shuffler::*
