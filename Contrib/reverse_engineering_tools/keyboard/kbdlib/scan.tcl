# Keyboard scan script for openMSX, driven by a job file written by
# kbdtool.py (which is sourced before this script and fills the 'job' array).
#
# Keys are only pressed in the key matrix (keymatrixdown/keymatrixup), and
# results are only read from VRAM. Nothing here depends on a unicodemap (the
# 'type' command) or on the character codes of the keyboard buffer
# (type_via_keybuf): those are what we want to find out.
#
# Two passes:
#
#  - chars:   each key (with each modifier combination) is pressed in the
#             BASIC screen editor, and the character codes on the top line
#             of the screen are read from VRAM.
#  - special: special keys and CTRL combinations don't show a character on
#             the screen, so a small BASIC program reads them with INKEY$ and
#             stores every character code in VRAM. The program is typed with
#             the keys that the chars pass found for its characters.
#
# Output files in job(out):
#   <pass>.txt    one line per key: "row bit combo code[,code...]", with -1 if
#                 nothing was typed; in the special pass, for a key that typed
#                 nothing, followed by "dead=code[,code...]" when the SPACE
#                 pressed after it typed something else than a space
#   dead.txt      for each key that typed nothing without modifiers in the
#                 chars pass, and each test letter (a e i o u c n): "row bit
#                 letter code[,code...]", what that key followed by the key
#                 for the letter typed (a dead key gives e.g. an a with an
#                 accent), or -1
#   font.txt      the font (pattern table), 8 bytes per character
#   status.txt    progress / errors
#   shot-*.png    screenshots in 'boot' mode, or on errors

set throttle off
set save_settings_on_exit off

proc status {msg} {
	set f [open [file join $::job(out) status.txt] w]; puts $f $msg; close $f
}
proc fail {msg} {
	status "ERROR: $msg"
	shot error {exit}
}
# A screenshot needs a rendered frame: briefly switch throttle on.
proc shot {name cmd} {
	set ::throttle on
	after time 0.3 [list screenshot -raw [file join $::job(out) shot-$name.png]]
	after time 0.5 "set ::throttle off; $cmd"
}

# --- pressing keys --------------------------------------------------------
# A key is a list of "row bit" pairs that are pressed together.
proc down {keys} { foreach {r b} $keys { keymatrixdown $r [expr {1 << $b}] } }
proc up {keys}   { foreach {r b} $keys { keymatrixup   $r [expr {1 << $b}] } }
# the last key is pressed a little after the others (the modifiers): some
# keyboard routines miss a combination that's pressed at the same moment
proc tap {keys cmd} {
	down [lrange $keys 0 end-2]
	after time 0.05 [list down [lrange $keys end-1 end]]
	after time 0.2 [list up $keys]
	after time 0.65 $cmd
}
# tap several lock keys one by one (pressing them together doesn't always work)
proc taps {list cmd} {
	if {![llength $list]} { eval $cmd; return }
	tap [lindex $list 0] [list taps [lrange $list 1 end] $cmd]
}

# --- booting --------------------------------------------------------------
proc boot {} {
	status booting
	if {$::job(boot_hold) ne ""} {
		down $::job(boot_hold)
		after time $::job(boot_hold_time) [list up $::job(boot_hold)]
	}
	if {[llength $::job(menu_keys)]} {
		after time $::job(menu_time) [list menu $::job(menu_keys)]
	}
	if {$::job(mode) eq "boot"} {
		foreach t $::job(shot_times) { after time $t [list shot [format %03d $t] {}] }
		after time [expr {[lindex $::job(shot_times) end] + 1}] { status done; exit }
		return
	}
	if {$::job(start_time) ne ""} {
		after time $::job(start_time) start
	} else {
		# (a start menu can also be a still screen: wait for BASIC only
		# after the menu keys)
		set t 10
		if {[llength $::job(menu_keys)]} {
			set m [expr {$::job(menu_time) + [llength $::job(menu_keys)]
			             * $::job(menu_interval) + 2}]
			if {$m > $t} { set t $m }
		}
		after time $t [list wait_ok {} 0]
	}
}
proc menu {keys} {
	if {![llength $keys]} {
		# (text to type at an MSX-DOS prompt: this one does use the
		# keyboard buffer, the keys aren't known yet at this point)
		if {$::job(menu_type) ne ""} { type_via_keybuf $::job(menu_type) }
		return
	}
	# (held a bit longer: some start menus read the keyboard only now and then)
	set k [lindex $keys 0]
	down $k
	after time 0.5 [list up $k]
	after time $::job(menu_interval) [list menu [lrange $keys 1 end]]
}
# MSX: wait until BASIC shows "Ok", or until the screen hasn't changed for
# 4 seconds (some BASICs end their start-up text differently); a start menu
# also gives a still screen: such machines need a recipe (see recipes.py)
proc wait_ok {previous stable} {
	set screen ""
	catch { set screen [get_screen] }
	if {[regexp -line {^\s*Ok\s*$} $screen]} { start; return }
	if {$screen eq $previous && [string trim $screen] ne ""} {
		incr stable
		if {$stable >= 2} { start; return }
	} else {
		set stable 0
	}
	if {[machine_info time] > 200} { fail "BASIC didn't start"; return }
	after time 2 [list wait_ok $screen $stable]
}

proc start {} {
	if {$::job(setup) ne ""} {
		# MSX: depending on the BIOS (and its settings), BASIC starts in
		# SCREEN 0 with 40 or 80 columns or in SCREEN 1: set up the same
		# text screen on all machines. (This uses the keyboard buffer: the
		# MSX BIOS character codes of these ASCII characters are the same
		# on all MSX machines; the check below makes sure it worked.)
		type_via_keybuf $::job(setup)
		after time 3 setup_check
	} else {
		start1
	}
}
proc setup_check {} {
	if {[get_screen_mode] ne "TEXT40"} {
		fail "not in SCREEN 0, WIDTH 40 after typing [string trim $::job(setup)]"
		return
	}
	start1
}
proc start1 {} {
	if {$::job(caps_tap) ne ""} { tap $::job(caps_tap) start2 } else { start2 }
}
proc start2 {} {
	dumpfont
	if {$::job(mode) eq "typetest"} {
		set ::out [open [file join $::job(out) typetest.txt] w]
		fconfigure $::out -buffering line
		status "typetest"
		typetest_next $::job(typetest)
		return
	}
	set ::passes $::job(passes)
	next_pass
}
proc next_pass {} {
	if {![llength $::passes]} {
		status done
		exit
		return
	}
	set ::passes [lassign $::passes pass]
	lassign $pass ::pass_name ::pass_method combos keys
	set ::jobs {}
	foreach combo $combos {
		foreach k $keys {
			if {$k in $::job(skip)} continue
			lappend ::jobs [list $combo {*}$k]
		}
	}
	set ::out [open [file join $::job(out) $::pass_name.txt] w]
	fconfigure $::out -buffering line
	status "scanning $::pass_name"
	if {$::pass_method eq "screen"} {
		screen_next
	} elseif {$::program_running} {
		inkey_block ""
	} else {
		type_program
	}
}

# job "combo row bit" -> output "row bit combo"
proc keyname {j} { lassign $j combo r b; return "$r $b $combo" }

# a combination like "SHIFT+CODE": the held keys and the tapped lock keys
proc held {combo} {
	set keys {}
	foreach m [split $combo +] {
		if {$m eq "none" || $m in $::job(locks)} continue
		lappend keys {*}$::job(mod,$m)
	}
	return $keys
}
proc locks {combo} {
	set list {}
	foreach m [split $combo +] {
		if {$m in $::job(locks)} { lappend list $::job(mod,$m) }
	}
	return $list
}
# press a key with held modifiers, then run cmd 'delay' seconds after the
# key; the modifiers are pressed one by one (pressing for example SHIFT and
# CODE at the same moment goes wrong on some machines, like the FS-A1GT)
proc press {combo r b delay cmd} {
	set mods [held $combo]
	set t 0
	foreach {mr mb} $mods {
		after time $t [list down [list $mr $mb]]
		set t [expr {$t + 0.1}]
	}
	after time [expr {$t + 0.05}] [list down [list $r $b]]
	after time [expr {$t + 0.15}] [list up [list $r $b]]
	after time [expr {$t + 0.2}] [list up $mods]
	after time [expr {$t + $delay}] $cmd
}

# --- method 'screen' ------------------------------------------------------
# the top line of the text screen (40 columns, see 'setup' in matrices.py)
proc raw_line {} {
	set base [expr {([vdpreg 2] & 0x7F) << 10}]
	set codes {}
	for {set c 0} {$c < 40} {incr c} { lappend codes [vpeek [expr {$base + $c}]] }
	return $codes
}
# Read the top line 10 times in a second, and run cmd with the codes that
# stayed the same: a blinking cursor changes, so it drops out, also when it
# has the same code as a typed character (like on the Sega SC-3000). Codes in
# job(screen_ignore) (spaces, a cursor that doesn't blink) are left out too.
proc read_stable {cmd {n 10} {samples {}}} {
	lappend samples [raw_line]
	if {[llength $samples] < $n} {
		after time 0.1 [list read_stable $cmd $n $samples]
		return
	}
	set codes {}
	for {set c 0} {$c < 40} {incr c} {
		set v [lindex $samples 0 $c]
		if {$v in $::job(screen_ignore)} continue
		set stable 1
		foreach line $samples { if {[lindex $line $c] != $v} { set stable 0 } }
		if {$stable} { lappend codes $v }
	}
	eval $cmd [list $codes]
}
proc screen_next {} {
	if {![llength $::jobs]} { close $::out; dead_test; return }
	set ::jobs [lassign $::jobs j]
	clear [list screen_key $j] 0
}
# clear the screen and wait until the top line is really empty (some screen
# editors redraw a little later)
proc clear {cmd n} {
	tap $::job(clear) [list clear_wait $cmd $n 0]
}
proc clear_wait {cmd n polls} {
	read_stable [list clear_check $cmd $n $polls] 5
}
proc clear_check {cmd n polls codes} {
	if {![llength $codes]} { eval $cmd; return }
	if {$polls < 6} { clear_wait $cmd $n [expr {$polls + 1}]; return }
	if {$n < 3} { clear $cmd [expr {$n + 1}]; return }
	fail "can't clear the screen (top line: $codes)"
}
proc screen_key {j} {
	taps [locks [lindex $j 0]] [list screen_press $j]
}
proc screen_press {j} {
	lassign $j combo r b
	set read [list read_stable [list screen_read $j]]
	if {[llength $::job(finish_key)]} {
		set cmd [list after time 0.4 [list tap $::job(finish_key) $read]]
	} else {
		set cmd [list after time 0.5 $read]
	}
	press $combo $r $b 0 $cmd
}
proc screen_read {j codes} {
	lassign $j combo r b
	if {![llength $codes] && $combo eq "none"} { lappend ::dead_todo [list $r $b] }
	screen_done $j [expr {[llength $codes] ? [join $codes ,] : -1}]
}
proc screen_done {j result} {
	puts $::out "[keyname $j] $result"
	# remember a key for each character, to type the BASIC program later
	lassign $j combo r b
	if {$combo in {none SHIFT} && [string is integer -strict $result]
	    && $result >= 0 && ![info exists ::key_for($result)]} {
		set ::key_for($result) [list $combo $r $b]
	}
	taps [locks $combo] screen_next
}

# keys that typed nothing: maybe a dead key, test them with letters (not all
# dead keys combine with all letters: e.g. a diaeresis only with u)
set dead_todo {}
proc dead_test {} {
	set ::deadout [open [file join $::job(out) dead.txt] w]
	set tests {}
	foreach k $::dead_todo {
		foreach {letter code} $::job(dead_letters) {
			if {[info exists ::key_for($code)]} { lappend tests [list {*}$k $letter $code] }
		}
	}
	set ::dead_todo $tests
	dead_next
}
proc dead_next {} {
	if {![llength $::dead_todo]} { close $::deadout; next_pass; return }
	set ::dead_todo [lassign $::dead_todo t]
	clear [list dead_press $t] 0
}
proc dead_press {t} {
	lassign $t r b letter code
	lassign $::key_for($code) lcombo lr lb
	press none $r $b 0.4 [list press $lcombo $lr $lb 0.5 [list read_stable [list dead_read $t]]]
}
proc dead_read {t codes} {
	lassign $t r b letter code
	puts $::deadout "$r $b $letter [expr {[llength $codes] ? [join $codes ,] : -1}]"
	dead_next
}

# --- method 'inkey' -------------------------------------------------------
set program_running 0
proc counter {} { vpeek $::job(counter) }
proc type_program {} {
	set b $::job(buffer); set c $::job(counter)
	set lines [list \
		"10 N=0:VPOKE $c,0" \
		"20 A\$=INKEY\$:IF A\$=\"\" THEN 20" \
		"30 VPOKE $b+N,ASC(A\$):N=N+1:IF N>255 THEN N=0" \
		"40 VPOKE $c,N:GOTO 20"]
	if {$::job(inkey_release)} {
		# INKEY$ gives the key that is pressed now (no keyboard buffer): wait
		# until it's released, otherwise one press is read several times
		lset lines end "40 VPOKE $c,N"
		lappend lines "50 IF INKEY\$<>\"\" THEN 50" "60 GOTO 20"
	}
	lappend lines "RUN"
	# the keys for the characters, as found in the chars pass
	set keys {}
	foreach line $lines {
		foreach ch [split $line ""] {
			if {$ch eq " "} { lappend keys [list none {*}$::job(space)]; continue }
			if {![dict exists $::job(program_codes) $ch]} {
				fail "no screen code known for '$ch' (see --charset)"; return
			}
			set code [dict get $::job(program_codes) $ch]
			if {![info exists ::key_for($code)]} {
				fail "no key found that types '$ch' (code $code) without modifiers or with SHIFT"
				return
			}
			lappend keys $::key_for($code)
		}
		lappend keys [list none {*}$::job(enter)]
	}
	status typing
	clear [list type_keys $keys] 0
}
proc type_keys {keys} {
	if {![llength $keys]} { after time 3 sanity; return }
	lassign [lindex $keys 0] combo r b
	press $combo $r $b $::job(type_interval) [list type_keys [lrange $keys 1 end]]
}
# SPACE must give code 32, otherwise the program isn't running
proc sanity {} {
	set before [counter]
	tap $::job(space) [list sanity2 $before]
}
proc sanity2 {before} {
	if {[counter] != $before && [vpeek [expr {$::job(buffer) + [counter] - 1}]] == 32} {
		set ::program_running 1
		inkey_block ""
	} else {
		fail "the BASIC program doesn't run (SPACE didn't give code 32)"
	}
}
# keys are scanned per combination; lock keys are tapped before and after
proc inkey_block {previous} {
	if {![llength $::jobs]} {
		taps [locks $previous] { close $::out; next_pass }
		return
	}
	set combo [lindex $::jobs 0 0]
	if {$combo ne $previous} {
		taps [locks $previous] [list taps [locks $combo] inkey_next]
	} else {
		inkey_next
	}
}
proc inkey_next {} {
	set ::jobs [lassign $::jobs j]
	lassign $j combo r b
	set before [counter]
	press $combo $r $b 1.0 [list inkey_check $j $before]
}
proc read_codes {before} {
	set codes {}
	for {set i $before} {$i != [counter]} {set i [expr {($i + 1) & 255}]} {
		lappend codes [vpeek [expr {$::job(buffer) + $i}]]
	}
	return $codes
}
proc inkey_check {j before} {
	set codes [read_codes $before]
	if {[llength $codes]} {
		puts $::out "[keyname $j] [join $codes ,]"
		inkey_block [lindex $j 0]
		return
	}
	# nothing typed: maybe a dead key; SPACE finishes it
	set before [counter]
	tap $::job(space) [list after time 0.4 [list inkey_dead $j $before]]
}
proc inkey_dead {j before} {
	set codes [read_codes $before]
	if {[llength $codes] && $codes ne "32"} {
		puts $::out "[keyname $j] -1 dead=[join $codes ,]"
	} else {
		puts $::out "[keyname $j] -1"
	}
	inkey_block [lindex $j 0]
}

# --- typetest: the 'type' command with the unicodemap -----------------------
# Per character: clear the screen, type it, and read the top line (like the
# 'screen' method). Output: "unicode code[,code...]".
# Tcl 8.6 can't handle characters above U+FFFF ('format %c' gives U+FFFD)
set big_chars_ok [expr {[scan [format %c 0x1F600] %c] == 0x1F600}]
proc typetest_next {list} {
	if {![llength $list]} { close $::out; status done; exit; return }
	set list [lassign $list u]
	if {$u > 0xFFFF && !$::big_chars_ok} {
		puts $::out "$u skipped"
		typetest_next $list
		return
	}
	clear [list typetest_type $u $list] 0
}
proc typetest_type {u list} {
	set ch [format %c $u]
	# with a space before it: a text starting with - would be taken as an
	# option of 'type' (the space itself is ignored on the screen)
	set text " $ch"
	if {[llength $::job(finish_key)]} { append text " " }
	type $text
	after time 1.5 [list read_stable [list typetest_read $u $list]]
}
proc typetest_read {u list codes} {
	puts $::out "$u [join $codes ,]"
	typetest_next $list
}

# --- font dump ------------------------------------------------------------
proc dumpfont {} {
	set base [expr {([vdpreg 4] & $::job(vdp_r4_mask)) * 0x800}]
	set f [open [file join $::job(out) font.txt] w]
	for {set c 0} {$c < 256} {incr c} {
		set l {}
		for {set y 0} {$y < 8} {incr y} { lappend l [vpeek [expr {$base + $c * 8 + $y}]] }
		puts $f $l
	}
	close $f
}

after realtime $job(timeout) { status "ERROR: timeout"; exit }
boot
