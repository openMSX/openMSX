# fail_after -- exits openMSX after timeout.
#
# This script should NOT be installed in the ~/.openmsx/share/scripts directory
# (because you don't want it to be active every time you start openMSX). Instead
# you should activate it via the openMSX command line with the option '-script
# fail_after.tcl'.
#
# Typically used in combination with the MSX-DOS 'omsxctl.com' utility.
#
# Adds two environment variables and two commands to openMSX;
#
# 'fail_after timeout [timeunit] [fail_id] [fail_code]'
# Schedules an openMSX exit after the timeout.
# This can be canceled by requesting a timeout of 0 or new timeout. 
# The timeunit can be selected between (msx)'time' and (host)'realtime'.
# The fail_id can be used to differentiate between multiple fail_after commands.
# The failure exit code can be given too. 
#
# 'fail_after_exit [fail_id] [fail_code]'
# Exits openMSX with an failure exit code and if the FAIL_AFTER_PATH is
# set it also creates a screenshot named from the failure id.
#
# Supported environment variables by this script;
#
# FAIL_AFTER_PATH=.
# Enabled automatic screenshots saving in case of failures in the supplied path.
#
# FAIL_AFTER_BOOT=30
# Enables the boot watchdog timer which will exit openMSX after the timeout(in seconds).
# To cancel this timer give an `fail_after 0` or any new fail_after command.
#

proc fail_after_exit {{fail_id "fail_after_exit"} {fail_code 2}} {
	global fail_after_path
	if {$fail_after_path != 0} {
		if {[catch {screenshot $fail_after_path/$fail_id.png} err_msg]} {
			puts stderr "warning: $err_msg"
		}
		# maybe later add; if {is_text_mode} { [get_screen] ?> $fail_after_path/$fail_id.scr }
	}
	puts stderr "error: Failure request from $fail_id"
	exit $fail_code
}

proc fail_after { timeout {time_unit "time"} {fail_id "fail_after"} {fail_code 2}} {
	global fail_after_prev_timer
	global fail_after_prev_id
	set msg ""
	if {$fail_after_prev_timer != 0} {
		after cancel $fail_after_prev_timer
		set msg "$fail_after_prev_id: Stopped attempt."
	}
	set fail_after_prev_id $fail_id
	if {$time_unit != "time"} {
		set time_unit "realtime"
	}
	if {$timeout != 0} {
		if {[catch {set fail_after_prev_timer [after $time_unit $timeout "fail_after_exit $fail_id $fail_code"]} err_msg]} {
			puts stderr "error: $err_msg"
			fail_after_exit fail_after_timer_error 1
		}
		set msg "$msg\n$fail_id: Automatic failure in $timeout $time_unit seconds."
	} else {
		set fail_after_prev_timer 0
	}
	return $msg
}

# Globals
set fail_after_prev_timer 0
set fail_after_prev_id 0
set fail_after_path 0

# Parse screenshot path env setting
if {[info exists ::env(FAIL_AFTER_PATH)] && ([string trim $::env(FAIL_AFTER_PATH)] != "")} {
	set fail_after_path [string trim $::env(FAIL_AFTER_PATH)]
}

# Enables boot watch dog timer when FAIL_AFTER_BOOT env has a value. (124 see `man timeout`) 
if {[info exists ::env(FAIL_AFTER_BOOT)] && ([string trim $::env(FAIL_AFTER_BOOT)] != "")} {
	fail_after [string trim $::env(FAIL_AFTER_BOOT)] realtime fail_after_boot 124
}
