set_help_text umrcallback \
{This is an example of a UMR callback function. You can set the "umr_callback"
setting in openMSX to "umrcallback" and this function will be called whenever
openMSX detects a read from uninitialized memory. It might be a good idea to
go into 'debug break' mode in this callback, so that you can easily examine
what is going on which has caused the UMR.

 @param addr The absolute address in the RAM device.
 @param name The name of the RAM device.
}

proc umrcallback {addr name} {
	puts stderr [format "UMR detected in RAM device \"$name\", offset: 0x%04X, PC: 0x%04X" $addr [reg PC]]
}


set_help_text vdpcmdinprogresscallback \
{This is an example of a vdpcmdinprogress callback function. To use it execute
    set vdpcmdinprogress_callback vdpcmdinprogresscallback
After changing that setting this proc will be called whenever openMSX detects a
write to a VDP command engine register while there's still a VDP command in
progress. Often this is an indication of a bug in the MSX program. This example
callback proc only prints the register number, the value that was being written
and the program counter. Feel free to write your own callback procs. For
example it might be a good idea to enter 'debug break' mode in your callback,
so that you can easily examine what is going on.

 @param reg The VDP command engine register number that was being written
            This is in range 0..14. Note that it's OK to write register 12
            while a command is in progress. So this callback will not be
            triggered for writes to register 12.
 @param val The value that was being written.
}

proc vdpcmdinprogresscallback {reg val} {
	puts stderr [format "Write to VDP command engine detected while there's still a command in progress: reg = %d, val = %d, PC = 0x%04X" [expr {$reg + 32}] $val [reg PC]]
}


set psg_directions_warning_printed false
proc psgdirectioncallback {} {
	if {$::psg_directions_warning_printed} return
	set ::psg_directions_warning_printed true
	message {The running MSX software has set unsafe PSG port directions.
Real (older) MSX machines can get damaged by this.} warning
}
set invalid_psg_directions_callback psgdirectioncallback


proc dihaltcallback {} {
	message "DI; HALT detected, which means a hang. You can just as well reset the MSX now..." warning
}
set di_halt_callback dihaltcallback
