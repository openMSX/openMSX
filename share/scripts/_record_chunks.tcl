namespace eval record_chunks {

set_help_text record_chunks \
{Records videos in doublesize format, but has extra options to record for a
limited amount of time and to chop up videos in chunks of a certain number of
seconds.

Usage:
    record_chunks [-chunktime <chunktime>] [-totaltime <totaltime> | -numchunks <numchunks>] start filename
    record_chunks stop

The chunktime parameter controls the maximum time for each chunk (default:
14:59 for YouTube) in seconds and the totaltime parameter controls the total
time to record in seconds (default: infinite). Instead of the totalchunks
parameter, you can also use the numchunks parameter to control the total time
as a multiple of the chunk time. The options are mutually exclusive.

The chunks are recorded with a chunk-number suffix behind filename. Do not use
an extension with filename, just a name is enough. Always provide the options
first.

Examples:
    record_chunks -numchunks 1 start simplegame
        Records a movie of 14:59 (maximum time for YouTube) to simplegame.avi
    record_chunks start longgame
        Records an infinite number of movies of 14:59 (maximum time for
        YouTube), until you use record_chunks stop. Names are longname01.avi,
        longname02.avi, etc.
    record_chunks -chunktime 60 -totaltime 230 start partgame
        Records movies of 1 minute until the total recorded time is 3:50, names
        are partgame01.avi, partgame02.avi, partgame03.avi and partgame04.avi.
}

variable filenamebase
variable iteration
variable chunk_time
variable total_time
variable next_after_id

proc record_chunks {args} {
	variable filenamebase
	variable iteration
	variable chunk_time
	variable total_time
	variable next_after_id

	# the defaults
	set chunk_time 899 ;# max time for a YouTube video
	set total_time -1  ;# record until someone says stop ..
	set num_chunks -1  ;# .. or till we recorded this many chunks

	while (1) {
		switch -- [lindex $args 0] {
			"-chunktime" {
				set chunk_time [lindex $args 1]
				set args [lrange $args 2 end]
			}
			"-totaltime" {
				set total_time [lindex $args 1]
				set args [lrange $args 2 end]
			}
			"-numchunks" {
				set num_chunks [lindex $args 1]
				set args [lrange $args 2 end]
			}
			"default" {
				break
			}
		}
	}

	if {($total_time > 0) && ($num_chunks > 0)} {
		error "You can't use both -numchunks and -totaltime options at the same time."
	}

	# do this outside of the loop, so that the order of options isn't too strict
	if {$num_chunks > 0} {
		set total_time [expr {$num_chunks * $chunk_time}]
	}

	switch -- [lindex $args 0] {
		"start" {
			if {[llength $args] != 2} {
				error "Expected another argument: the name of your video!"
			}
			if {[dict get [record status] status] ne "idle"} {
				error "Already recording!"
			}
			set filenamebase [lindex $args 1]
			set iteration 0
			record_next_part
		}
		"stop" {
			if {[llength $args] != 1} {
				error "Too many arguments. Stop is just stop."
			}
			if {![info exists record_chunks::iteration]} {
				error "No recording in progress..."
			}
			after cancel $next_after_id
			stop_recording
		}
		"default" {
			error "Syntax error in command."
		}
	}
}

proc stop_recording {} {
	variable iteration
	unset iteration
	record stop
	puts "Stopped recording..."
}

proc record_next_part {} {
	variable iteration
	variable next_after_id
	variable filenamebase
	variable chunk_time
	variable total_time

	set cmd record_chunks::record_next_part
	set time_to_record $chunk_time
	if {$total_time > 0} {
		set time_left [expr {$total_time - ($chunk_time * $iteration)}]
		if {$time_left <= $chunk_time} {
			set cmd record_chunks::stop_recording
			set time_to_record $time_left
		}
	}

	record stop

	incr iteration
	if {$iteration == 1 && $cmd eq "record_chunks::stop_recording"} {
		# if we're only going to record one movie, no need to number it
		set fname $filenamebase
	} else {
		set fname [format "%s%02d" $filenamebase $iteration]
	}
	record start -doublesize $fname

	set next_after_id [after time $time_to_record $cmd]
	puts "Recording to $fname for [utils::format_time $time_to_record]..."
}

namespace export record_chunks

} ;# namespace record_chunks

namespace import record_chunks::*
