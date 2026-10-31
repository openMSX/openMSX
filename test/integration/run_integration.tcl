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
#   OPENMSX_BIN=/path/to/openmsx tclsh test/integration/run_integration.tcl

set here [file dirname [file normalize [info script]]]
set suites {
	step_back_integration_test.tcl
}

set failed 0
foreach s $suites {
	set path [file join $here $s]
	puts "=== $s ==="
	if {[catch {exec tclsh $path 2>@1} msg]} {
		puts stdout $msg
		puts stderr "--- $s FAILED ---"
		set failed 1
	} else {
		puts stdout $msg
	}
	puts ""
}
if {$failed} {
	puts stderr "Integration suites FAILED"
	exit 1
}
puts "All integration suites passed"
exit 0
