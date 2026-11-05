# test/common/tcltest.tcl
#
# A tiny, dependency-free assertion / test framework for Tcl.
# It runs in plain tclsh (no openMSX required) and provides:
#   - tcltest::new      : create a fresh test context binding results to a var
#   - tcltest::test     : run one test case
#   - tcltest::assert*  : assertion helpers that fail the current test
#   - tcltest::summary  : print a pass/fail summary
#
# A test is a named block of code. Assertions record failures; any Tcl error
# thrown by the block also counts as a failure. Each test is reported as it
# runs (so a hang can be located) and tallied at the end.
#
# Example:
#   source tcltest.tcl
#   set ctx [tcltest::new testctx]
#   tcltest::test $ctx "my test" {
#       tcltest::eq $ctx [string length "hello"] 5 "length"
#       tcltest::is $ctx {2+2 == 4} "two plus two"
#   }
#   if {[tcltest::fails $ctx]} { exit 1 }
#   exit 0
#
# Note: every helper takes the context NAME (a string) as its first argument;
# the context holds its state in a global variable with that name.
# For openMSX integration tests the same procs can be loaded inside a
# `-script`; in that case use `tcltest::summary` to print and continue.

namespace eval tcltest {}

# Global registry for all tests (for listing/filtering)
if {![info exists ::__tcltest_registry]} { set ::__tcltest_registry [list] }
if {![info exists ::__tcltest_filter]} {
	if {[info exists env(TCLTEST_FILTER)] && $env(TCLTEST_FILTER) ne ""} {
		set ::__tcltest_filter $env(TCLTEST_FILTER)
	} elseif {[info exists ::argv] && [llength $::argv] > 0} {
		set ::__tcltest_filter [lindex $::argv 0]
	} else {
		set ::__tcltest_filter ""
	}
}

if {![info exists ::__tcltest_ran_any]} { set ::__tcltest_ran_any 0 }

# tcltest::new <ctxvarname>  -- initialize context (a dict stored in a global
# named <ctxvarname>) and return the name so callers can write:
#     set ctx [tcltest::new ctx]
# and then pass the name $ctx to tcltest::test / the assertion helpers.
proc tcltest::new {ctx} {
	upvar #0 $ctx C
	set C [dict create total 0 failed 0 failures [list]]
	return $ctx
}

# tcltest::register <name> -- register a test name in global registry
proc tcltest::register {name} {
	if {[lsearch -exact $::__tcltest_registry $name] < 0} {
		lappend ::__tcltest_registry $name
	}
	return $name
}

# tcltest::should_run <name> -- check if test should run based on filter
proc tcltest::should_run {name} {
	if {$::__tcltest_filter eq ""} { return 1 }
	if {$::__tcltest_filter eq "--list-tests" || $::__tcltest_filter eq "-l"} { return 0 }
	# match by exact name or by substring
	if {$name eq $::__tcltest_filter} { return 1 }
	if {[string match "*$::__tcltest_filter*" $name]} { return 1 }
	return 0
}

# tcltest::test <ctx> <name> <script>
#   Resets the failure list for this test, runs <script>, and counts the
#   outcome. If <script> itself throws, that is a failure too. The script is
#   evaluated in the caller's scope (so it can use upvar'd vars).
proc tcltest::test {ctx name script} {
	tcltest::register $name
	if {$::__tcltest_filter eq "--list-tests" || $::__tcltest_filter eq "-l"} {
		puts "TEST  $name"
		return
	}
	if {![tcltest::should_run $name]} { return }
	set ::__tcltest_ran_any 1
	upvar #0 $ctx C
	dict set C total [expr {[dict get $C total] + 1}]
	set local [dict create nfailed 0 err ""]
	# record per-test failures in a side channel
	set ::__tcltest_fail_$ctx [list]
	set failed 0
	if {[catch {uplevel 1 $script} err]} {
		set failed 1
		dict set C err "$name: error: $err"
		set cmt [list "$name: error: $err"]
	} else {
		set cmt [set ::__tcltest_fail_$ctx]
		if {[llength $cmt]} { set failed 1 }
	}
	if {$failed} {
		dict set C failed [expr {[dict get $C failed] + 1}]
		dict set C failures [concat [dict get $C failures] $cmt]
		puts "FAIL  $name"
		foreach f $cmt { puts "        $f" }
	} else {
		puts "ok    $name"
	}
}

# tcltest::_fail <ctx> <msg>  -- record a failure in the current test
proc tcltest::_fail {ctx msg} {
	if {[info exists ::__tcltest_fail_$ctx]} {
		lappend ::__tcltest_fail_$ctx $msg
	}
}

# Assertions. All take the current test context as their first argument.
proc tcltest::is {ctx cond msg} {
	if {![uplevel 1 [list expr $cond]]} {
		tcltest::_fail $ctx "expected condition true: {$cond} ($msg)"
	}
}

proc tcltest::not {ctx cond msg} {
	if {[uplevel 1 [list expr $cond]]} {
		tcltest::_fail $ctx "expected condition false: {$cond} ($msg)"
	}
}

proc tcltest::eq {ctx actual expected msg} {
	if {![string equal $actual $expected]} {
		tcltest::_fail $ctx "expected '{$expected}' but got '{$actual}' ($msg)"
	}
}

# eq_hex compares as unsigned hex numbers (case-insensitive), ignoring leading
# zeros, e.g. for Z80 register values like BC, PC, HL, DE. Both arguments may
# be decimal integers (registers come back as ints), 0x-prefixed hex literals,
# or hex strings like "F000".
proc tcltest::eq_hex {ctx actual expected msg} {
	set a [tcltest::_hex $actual]
	set e [tcltest::_hex $expected]
	if {$a ne $e} {
		tcltest::_fail $ctx "expected 0x$e but got 0x$a ($msg)"
	}
}

# tcltest::_hex <v>  -- normalize an integer/hex representation to lowercase
# hex without leading zeros (or a leading 0x prefix).
proc tcltest::_hex {v} {
	if {[string is integer -strict $v]} {
		return [format %x $v]
	}
	set s [string tolower $v]
	if {[string match "0x*" $s]} { set s [string range $s 2 end] }
	set s [string trimleft $s "0"]
	if {$s eq ""} { set s "0" }
	return $s
}

# tcltest::eq_fp compares floating point values within a relative/absolute
# tolerance, similar in style to tcltest::eq_hex. Both values may be strings
# returned by openMSX.
proc tcltest::eq_fp {ctx actual expected msg {tol 1e-6}} {
	if {![string is double -strict $actual]} {
		tcltest::_fail $ctx "expected a number but got '$actual' ($msg)"
		return
	}
	if {![string is double -strict $expected]} {
		tcltest::_fail $ctx "expected a number but got '$expected' ($msg)"
		return
	}
	set a [expr {double($actual)}]
	set e [expr {double($expected)}]
	set diff [expr {abs($a - $e)}]
	set scale [expr {abs($e)}]
	if {$scale < 1.0} { set scale 1.0 }
	if {$diff > [expr {$tol * $scale}]} {
		tcltest::_fail $ctx "expected $expected but got $actual ($msg)"
	}
}

proc tcltest::fails {ctx} {
	upvar #0 $ctx C
	expr {[dict get $C failed] > 0}
}

# tcltest::summary <ctx>  -- print totals; returns number of failures
proc tcltest::list_tests {} {
	foreach t $::__tcltest_registry {
		puts "TEST  $t"
	}
}

proc tcltest::summary {ctx} {
	if {$::__tcltest_filter eq "--list-tests" || $::__tcltest_filter eq "-l"} {
		return 0
	}
	if {$::__tcltest_filter ne ""} {
		if {[info exists ::__tcltest_ran_any] && !$::__tcltest_ran_any} {
			puts ""
			puts "No tests matched filter"
			return 1
		}
	}
	upvar #0 $ctx C
	set total [dict get $C total]
	set failed [dict get $C failed]
	puts ""
	puts "$failed of $total tests FAILED"
	return $failed
}
