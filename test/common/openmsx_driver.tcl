# test/common/openmsx_driver.tcl
#
# A thin Tcl driver that controls a HEADLESS openMSX through the XML
# "stdio" control protocol (openMSX started with `-control stdio`, which
# leaves the renderer uninitialized (dummy) and shows no window -- see
# doc/manual/openmsx-control.html).
#
# Responsibilities:
#   * spawn / close an openMSX subprocess with a configurable command line
#   * send console commands as <command> XML elements and decode the
#     corresponding <reply result="..."> responses (in order, with timeout)
#   * convenience helpers for the things the integration tests need:
#     reading CPU registers, setting/removing breakpoints, ensuring the
#     reverse feature is collecting, invoking step_back, and exiting.
#
# Each integration test should run a FRESH emulator (omsx::spawn) so tests
# are independent.
#
# Note: reply text can contain XML entities; the driver decodes the common
# named entities. This is enough for the register/address values used in
# assertions.

namespace eval omsx {
	variable pipe ""
	variable ibuf ""          ;# accumulated unparsed bytes from the subprocess
	variable timeout_ms 60000 ;# generous: reverse step_back can be slow
	variable cmd  ""          ;# canonical path to the openmsx binary
	variable opened 0
	variable rootdir ""       ;# repository root (captured at source time)
}

set ::omsx::pipe ""
set ::omsx::ibuf ""          ;# accumulated unparsed bytes from the subprocess
set ::omsx::timeout_ms 60000 ;# generous: reverse step_back can be slow
set ::omsx::cmd  ""          ;# canonical path to the openmsx binary
set ::omsx::opened 0
# Capture the repository root at source time: inside procs [info script] no
# longer returns this driver's path. This driver lives in <root>/test/common.
set ::omsx::rootdir [file normalize [file join [file dirname [file normalize [info script]]] .. ..]]

# omsx::xml_escape  -- escape text for embedding inside a <command> element
proc omsx::xml_escape {s} {
	return [string map [list {&} {&amp;} {<} {&lt;} {>} {&gt;}] $s]
}
# omsx::xml_decode  -- decode the entities openMSX puts into reply/log text
proc omsx::xml_decode {s} {
	return [string map [list {&lt;} {<} {&gt;} {>} {&quot;} {"} {&apos;} {'} {&amp;} {&}] $s]
}

# omsx::find_binary  -- locate the openmsx executable.
# Honour an OPENMSX_BIN env override, then the in-tree build, then search
# $PATH (which may be unset in minimal shells), then common install prefixes.
proc omsx::find_binary {} {
	if {$::omsx::cmd ne ""} { return $::omsx::cmd }
	if {[info exists env(OPENMSX_BIN)] && $env(OPENMSX_BIN) ne ""} {
		set ::omsx::cmd $env(OPENMSX_BIN)
		return $env(OPENMSX_BIN)
	}
	# In-tree build tree: `make` puts the binary in ./derived/openmsx relative to
	# the repository root (this driver lives in <root>/test/common). Prefer it so
	# developers test the binary they just built without installing.
	set derived [file join $::omsx::rootdir derived openmsx]
	if {[file executable $derived]} {
		set ::omsx::cmd $derived
		return $derived
	}
	set path {}
	if {[info exists env(PATH)]} { set path $env(PATH) } else { set path /usr/local/bin:/usr/bin:/bin }
	foreach d [split $path :] {
		set f [file join $d openmsx]
		if {[file executable $f]} { set ::omsx::cmd $f; return $f }
	}
	# Common install prefixes (openMSX's default is /opt/openMSX)
	foreach d {/opt/openMSX/bin /opt/openmsx/bin /usr/local/bin /usr/bin /bin} {
		set f [file join $d openmsx]
		if {[file executable $f]} { set ::omsx::cmd $f; return $f }
	}
	return openmsx
}

# omsx::spawn <clargs...>  -- start openMSX with `-control stdio` plus <clargs>
# Returns the channel. The element is NOT yet at <openmsx-control> until the
# first command is sent (omsx::cmd sends the opening tag on demand).
proc omsx::spawn {args} {
	if {$::omsx::pipe ne ""} { omsx::exit }
	set cl [omsx::find_binary]

	# Force openMSX to load the repository's own Tcl scripts instead of whatever
	# is installed in the system. openMSX's data_file() resolves a file from
	# OPENMSX_USER_DATA first, then OPENMSX_SYSTEM_DATA.
	#
	# We point OPENMSX_USER_DATA at a SCRATCH directory (test/.userdata) that
	# contains a `scripts` symlink to <repo>/share/scripts, rather than pointing
	# it at <repo>/share itself: openMSX treats the user-data dir as writable and
	# would otherwise drop settings.xml / .filecache into the repo's share/.
	# scripts/_disasm.tcl then resolves from the working tree through the symlink.
	# OPENMSX_SYSTEM_DATA is deliberately left UNSET so everything the source
	# tree does not ship (machine definitions like C-BIOS_MSX1.xml, system ROMs)
	# still falls back to the installed system data dir.
	set repo_share [file join $::omsx::rootdir share]
	if {[file isdirectory [file join $repo_share scripts]]} {
		set userdata [file join $::omsx::rootdir test .userdata]
		file mkdir $userdata
		set scripts_link [file join $userdata scripts]
		set want [file join $repo_share scripts]
		# (Re)create the symlink when it is missing, dangling, or points at
		# a different checkout. NOTE: 'file exists' follows symlinks, so it
		# returns false for a DANGLING link while 'file link' (create) still
		# fails with "path already exists" -- testing existence alone can
		# therefore neither detect nor repair a stale link (e.g. left over
		# from a renamed/moved working tree). Compare the link target
		# instead, and delete-then-relink on any mismatch.
		if {[catch {file link $scripts_link} curTarget] || ($curTarget ne $want)} {
			file delete -force $scripts_link
			file link $scripts_link $want
		}
		set ::env(OPENMSX_USER_DATA) $userdata
		unset -nocomplain ::env(OPENMSX_SYSTEM_DATA)
	}

	set stderr [file join $::omsx::rootdir test .stderr.log]
	set cmdline "$cl -control stdio [join $args { }] 2> [list $stderr]"
	set ::omsx::pipe [open "|$cmdline" r+]
	fconfigure $::omsx::pipe -blocking 0 -buffering none
	set ::omsx::ibuf ""
	set ::omsx::opened 0
	return $::omsx::pipe
}

# omsx::sleepms <ms>  -- real blocking sleep (after alone is a timer that only
# fires when an event loop runs; vwait drives the event loop).
proc omsx::sleepms {ms} {
	if {$ms <= 0} { return }
	after $ms { set ::omsx::__awake 1 }
	vwait ::omsx::__awake
	unset ::omsx::__awake
}

proc omsx::_drain {} {
	while {1} {
		set chunk [read $::omsx::pipe 4096]
		if {$chunk eq ""} { break }
		append ::omsx::ibuf $chunk
	}
}

# omsx::_next_reply  -- try to extract one complete <reply> from the buffer.
# Returns {result content} on success, or "" if not enough data yet. Consumed
# bytes (up to and including that </reply>) are dropped, so <log>/<update>
# and the outer <openmsx-output> wrapper are skipped automatically.
proc omsx::_next_reply {} {
	# find a complete <reply...>...</reply> element
	if {[regexp -indices -- {<reply( [^>]*)>(.*?)</reply>} $::omsx::ibuf -> ai ci]} {
		set a0 [lindex $ai 0]; set a1 [lindex $ai 1]
		set c0 [lindex $ci 0]; set c1 [lindex $ci 1]
		set cut [expr {$c1 + 9}]   ;# + len("</reply>")
		set attrs [string range $::omsx::ibuf $a0 $a1]
		set content [omsx::xml_decode [string range $::omsx::ibuf $c0 $c1]]
		set ::omsx::ibuf [string range $::omsx::ibuf $cut end]
		set result ok
		if {[regexp {result="nok"} $attrs]} { set result nok }
		return [list $result $content]
	}
	return ""
}

# omsx::cmd <command-string>  -- execute a console command, return {result content}
# result is "ok", "nok" (command error) or "timeout".
proc omsx::cmd {command} {
	if {!$::omsx::opened} {
		puts -nonewline $::omsx::pipe "<openmsx-control>"
		flush $::omsx::pipe
		set ::omsx::opened 1
	}
	puts -nonewline $::omsx::pipe "<command>[omsx::xml_escape $command]</command>"
	flush $::omsx::pipe
	set deadline [expr {[clock milliseconds] + $::omsx::timeout_ms}]
	while {1} {
		omsx::_drain
		set r [omsx::_next_reply]
		if {$r ne ""} { return $r }
		if {[clock milliseconds] > $deadline} {
			return [list timeout [string range $::omsx::ibuf 0 200]]
		}
		omsx::sleepms 2
	}
}

# omsx::ok <command-string>  -- like cmd, but abort the script on error
proc omsx::ok {command} {
	set r [omsx::cmd $command]
	if {[lindex $r 0] ne "ok"} {
		error "omsx command failed ({$command}): [lindex $r 1]"
	}
	return [lindex $r 1]
}

# omsx::reg <name>  -- read a CPU register (returns its decimal value string)
proc omsx::reg {name} { return [omsx::ok "reg $name"] }

# omsx::reg_hex <name>  -- read a CPU register formatted as 0xXXXX
proc omsx::reg_hex {name} {
	set addr [omsx::reg $name]
	return [format 0x%04X $addr]
}

# omsx::set_reg <name> <value>  -- write a CPU register
proc omsx::set_reg {name value} { omsx::ok "reg $name $value" }

# omsx::power_on / omsx::reset  -- machine control helpers
proc omsx::power_on {} { omsx::ok "set power on" }
proc omsx::reset {} { omsx::ok "reset" }

# omsx::break_cpu / omsx::continue  -- debug break/continue
proc omsx::break_cpu {} { omsx::ok "debug break" }
proc omsx::continue_cpu {} { omsx::ok "debug cont" }
proc omsx::is_breaked {} { expr {[omsx::ok "debug breaked"]} }

# omsx::instr_at <addr>  -- mnemonic of the instruction about to execute
proc omsx::instr_at {addr} {
	set d [omsx::ok "debug disasm $addr"]
	return [lindex $d 0]
}

# omsx::set_bp <addr> [<cond>]  -- set a breakpoint; returns its id (e.g. bp#1)
proc omsx::set_bp {addr args} {
	set cond {}
	if {[llength $args] > 0} { set cond [lindex $args 0] }
	if {$cond eq ""} {
		return [omsx::ok "debug set_bp $addr"]
	}
	return [omsx::ok "debug set_bp $addr {$cond}"]
}

# omsx::rm_bp <id>  -- remove a breakpoint by id
proc omsx::rm_bp {id} { omsx::ok "debug remove_bp $id" }

# omsx::ensure_reverse  -- make sure the reverse feature is collecting
proc omsx::ensure_reverse {} {
	set st [omsx::ok "reverse status"]
	if {![string match "*status enabled*" $st]} {
		omsx::ok "reverse start"
	}
}

# omsx::machine_time  -- current emulated machine time (in seconds)
proc omsx::machine_time {} {
	return [omsx::ok "machine_info time"]
}

# omsx::step_back  -- invoke the console step_back (from _disasm.tcl)
proc omsx::step_back {} {
	set r [omsx::cmd "step_back"]
	if {[lindex $r 0] ne "ok"} {
		error "step_back failed: [lindex $r 1]"
	}
}

# omsx::step_in  -- invoke the console step_in (from _disasm.tcl)
proc omsx::step_in {} {
	set r [omsx::cmd "step_in"]
	if {[lindex $r 0] ne "ok"} {
		error "step_in failed: [lindex $r 1]"
	}
}

# omsx::exit  -- shut down the emulator
proc omsx::exit {} {
	if {$::omsx::pipe eq ""} { return }
	catch { omsx::cmd "exit" }
	omsx::sleepms 300
	catch { close $::omsx::pipe }
	set ::omsx::pipe ""
}

# omsx::wait_boot <ms>  -- let the machine run for a while after power-on
proc omsx::wait_boot {ms} { omsx::sleepms $ms }
