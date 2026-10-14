#!/usr/bin/env tclsh
# test/integration/run_integration.tcl
#
# Launcher for the openMSX headless integration tests.
#
# The tests drive a REAL openMSX instance through the console XML "stdio"
# control protocol, so they need:
#   * an openmsx binary (set OPENMSX_BIN, use the in-tree build, or have one
#     on $PATH), and
#   * the "C-BIOS_MSX1" machine definition (shipped with openMSX).
#
# Every test file ends with its own summary + non-zero exit on failure, so
# this launcher runs each suite as a separate tclsh subprocess and aggregates
# the exit codes (sourcing them in one interpreter would stop at the first
# `exit`).
#
# Usage:
#   tclsh test/integration/run_integration.tcl        # all suites
#   tclsh test/integration/run_integration.tcl --list-tests
#   tclsh test/integration/run_integration.tcl ldir_afterblock
#   OPENMSX_BIN=/path/to/openmsx tclsh test/integration/run_integration.tcl

set here [file dirname [file normalize [info script]]]
set suites {
	step_back_integration_test.tcl
}

set filter ""
if {[info exists ::argv] && [llength $::argv] > 0} {
	set filter [lindex $::argv 0]
}

if {$filter eq "--list-tests" || $filter eq "-l"} {
	set failed 0
	foreach s $suites {
		set path [file join $here $s]
		set out [exec tclsh $path --list-tests 2>@1]
		puts $out
		if {[catch {set tmp $out}]} { set failed 1 }
	}
	if {$failed} { exit 1 }
	exit 0
}

set failed 0
foreach s $suites {
	set path [file join $here $s]
	if {$filter ne "" && $filter ne "--list-tests" && $filter ne "-l"} {
		puts "=== $s (filter: $filter) ==="
		if {[catch {exec env TCLTEST_FILTER=$filter tclsh $path 2>@1} output]} {
			puts stdout $output
			if {[string match "*No tests matched filter*" $output]} {
				puts stderr "No tests matched filter '$filter'"
				set failed 1
			} else {
				puts stderr "--- $s FAILED ---"
				set failed 1
			}
		} else {
			puts $output
			if {[string match "*No tests matched filter*" $output]} {
				puts stderr "No tests matched filter '$filter'"
				set failed 1
			} elseif {[regexp -line {([0-9]+) of ([0-9]+) tests FAILED} $output -> f t]} {
				if {$f > 0} { set failed 1 }
			}
		}
	} elseif {$filter eq "--list-tests" || $filter eq "-l"} {
		set out [exec tclsh $path --list-tests 2>@1]
		puts $out
	} else {
		puts "=== $s ==="
		if {[catch {exec tclsh $path 2>@1} msg]} {
			puts stdout $msg
			puts stderr "--- $s FAILED ---"
			set failed 1
		} else {
			puts stdout $msg
		}
	}
	puts ""
}
if {$failed} {
	puts stderr "Integration suites FAILED"
	exit 1
}
puts "All integration suites passed"
exit 0
