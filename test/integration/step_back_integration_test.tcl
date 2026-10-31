# test/integration/step_back_integration_test.tcl
#
# Headless openMSX integration tests for the `step_back` improvements in
# share/scripts/_disasm.tcl (block-repeat instructions: LDIR/LDDR/CPIR/CPDR/
# INIR/INDR/OTIR/OTDR).
#
# These run on a REAL emulated Z80 via the openMSX console XML "stdio"
# control protocol (see test/common/openmsx_driver.tcl). They use the
# cartridge ROM fixtures in test/roms/:
#
#   ldir_irq.rom
#       LDIR (ED B0) @ 0x401B with BC=$2000 (8192 iters), HL=$4000 source,
#       DE=$C000 dest; a video IRQ interrupts it mid-way. A `JR FOREVER` sits
#       @ 0x401D right after the block.
#
#   otir_irq.rom
#       OTIR (ED B3) @ 0x4019 with B=0 (256 iters), C=$A0 (PSG port). This is
#       the B-only counter case. A `JR FOREVER` sits @ 0x401B after it.
#
#   ldir_djnz_loop.rom
#       LDIR @ 0x4019 inside a DJNZ outer loop (4 passes, DI so no IRQ);
#       after the last pass a `JR FOREVER` @ 0x401E. Exercises a block
#       re-executed at the same PC across loop passes.
#
#   ldir_varying_bc.rom
#       Shared LDIR subroutine @ 0x4027 called with a different BC per call
#       site (5, then 3, then 4; DI so no IRQ); same block PC with varying
#       initial counters.
#
#   ldir_nested_inir.rom
#       Outer LDIR @ 0x4026 (BC=$2000) interrupted by video IRQs whose H.TIMI
#       hook runs its own INIR, so nested blocks at different PCs suspend and
#       resume the outer execution.
#
# The KEY regression these guard: previously step_back only recognised a
# block-repeat instruction as "current" -- it did NOT rewind past a block
# that had JUST completed (the instruction immediately AFTER the block), and
# it did not reliably undo an interrupted block to before its first
# iteration. The block tests below call step_back from a block or from the
# instruction just after it, and assert we land on the block PC with the
# registers restored to their ORIGINAL (pre-block) values.
#
# Requires an openMSX binary (see run_integration.tcl). This file is
# standalone so it can be run directly:
#   tclsh test/integration/step_back_integration_test.tcl

set here [file dirname [file normalize [info script]]]
source [file join $here .. common tcltest.tcl]
source [file join $here .. common openmsx_driver.tcl]

namespace eval it {}

set ::it::romdir [file normalize [file join $here .. roms]]

# it::rom <basename>  -- full path to a rom fixture
proc it::rom {basename} { return [file join $::it::romdir $basename] }

# it::boot_cart <rom>  -- spawn, power on, wait until the cart is mapped at
# page 1 (0x4000 == 0x41). Returns when the cartridge is live.
proc it::boot_cart {rom} {
	omsx::spawn -machine C-BIOS_MSX1 -cart $rom
	omsx::sleepms 600
	omsx::power_on
	omsx::ensure_reverse
	set dl [expr {[clock milliseconds] + 20000}]
	while {1} {
		set v [omsx::ok "debug read memory 0x4000"]
		if {$v eq "65"} { break }        ;# 0x41 = 'A' (cart header)
		if {[clock milliseconds] > $dl} { error "cart never mapped at 0x4000" }
		omsx::sleepms 150
	}
	omsx::sleepms 100
}

# it::run_to_bp  -- reset so the cart re-runs (block executes again), then
# continue so the machine actually runs, and wait until the CPU is breaked
# by a breakpoint. (reset itself leaves the CPU stalled at the reset vector
# in the debug/control session, hence the explicit `debug cont`.)
proc it::run_to_bp {} {
	set dl [expr {[clock milliseconds] + 30000}]
	omsx::reset
	omsx::continue_cpu
	while {1} {
		if {[omsx::is_breaked]} { break }
		if {[clock milliseconds] > $dl} { error "CPU never reached breakpoint (breaked=[omsx::is_breaked])" }
		omsx::sleepms 50
	}
}

proc it::reg {name} { return [omsx::reg $name] }

# it::cont_to_bp  -- like run_to_bp but WITHOUT reset: continue from the
# current breaked position (e.g. a previous breakpoint hit) and wait for
# the next hit of a persistent breakpoint. Used to observe the 2nd, 3rd,
# ... firing of one breakpoint without re-running the ROM.
proc it::cont_to_bp {} {
	set dl [expr {[clock milliseconds] + 30000}]
	omsx::continue_cpu
	while {1} {
		if {[omsx::is_breaked]} { break }
		if {[clock milliseconds] > $dl} { error "CPU never reached breakpoint (breaked=[omsx::is_breaked])" }
		omsx::sleepms 50
	}
}

# it::scan_ldirs <rom> -- scan a ROM image for 'LD BC,nn; LDIR' pairs.
# Returns a list of {ldir_addr bc_value} in CPU address space (the image is
# mapped at 0x4000). This lets the many-random-BC test derive the expected
# sizes from the ROM itself instead of duplicating the table in Tcl.
proc it::scan_ldirs {rom} {
	set f [open $rom rb]
	set data [read $f]
	close $f
	binary scan $data H* hex
	set hex [string tolower $hex]
	set result {}
	set pos 0
	while {1} {
		set h [string first "edb0" $hex $pos]
		if {$h < 0} break
		set i [expr {$h / 2}]
		if {($i >= 3) &&
		    ([string range $hex [expr {2 * ($i - 3)}] [expr {2 * ($i - 3) + 1}]] eq "01")} {
			set lo [scan [string range $hex [expr {2 * ($i - 2)}] [expr {2 * ($i - 2) + 1}]] %x]
			set hi [scan [string range $hex [expr {2 * ($i - 1)}] [expr {2 * ($i - 1) + 1}]] %x]
			lappend result [list [expr {0x4000 + $i}] [expr {$lo + 256 * $hi}]]
		}
		set pos [expr {$h + 2}]
	}
	return $result
}

set ctx [tcltest::new step_back_integration]

# --- Test cases -----------------------------------------------------------

# 1) LDIR, after-block (the regression bug): break at the JR right after the
#    block; step_back must rewind past the COMPLETED block to its start.
tcltest::test $ctx "ldir after-block (step_back past finished block)" {
	it::boot_cart [it::rom ldir_irq.rom]
	set bp [omsx::set_bp "0x401d"]              ;# JR FOREVER (after LDIR)
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x401d "break fires after block"
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "lands on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x2000 "BC restored to 8192"
	tcltest::eq_hex $ctx [it::reg hl] 0x4000 "HL restored to source"
	tcltest::eq_hex $ctx [it::reg de] 0xc000 "DE restored to dest"
	omsx::rm_bp $bp
	omsx::exit
}

# 2) LDIR, mid-block (interrupted by IRQ): a condition breakpoint fires
#    halfway through the LDIR (BC==0x1000); step_back must still rewind to
#    before the FIRST iteration (BC=$2000).
tcltest::test $ctx "ldir mid-block interrupted by IRQ rewinds to first iteration" {
	it::boot_cart [it::rom ldir_irq.rom]
	set bp [omsx::set_bp "0x401b" {[reg BC] == 0x1000}]
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "break fires on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x1000 "BC mid-block (0x1000)"
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "lands on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x2000 "BC restored to 8192 (first iteration)"
	tcltest::eq_hex $ctx [it::reg hl] 0x4000 "HL restored to source"
	tcltest::eq_hex $ctx [it::reg de] 0xc000 "DE restored to dest"
	omsx::rm_bp $bp
	omsx::exit
}

# 3) OTIR, after-block (B-only counter): breaking after the OTIR and stepping
#    back must land on the OTIR with B=0 (256 iters) restored.
tcltest::test $ctx "otir after-block B-only counter reverts to B=0" {
	it::boot_cart [it::rom otir_irq.rom]
	set bp [omsx::set_bp "0x401b"]              ;# JR FOREVER (after OTIR)
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "break fires after OTIR"
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg pc] 0x4019 "lands on OTIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x00a0 "B=0,C=0xA0 preserved (256 iters)"
	omsx::rm_bp $bp
	omsx::exit
}

# 4) LDIR inside a DJNZ outer loop (4 passes, DI so no IRQ): the block PC is
#    the same on every pass. Breaking mid-LDIR and stepping back must land on
#    the first iteration of the CURRENT block execution (BC=$2000), never an
#    earlier loop pass. (The loop structure must not break step_back on real
#    hardware.)
tcltest::test $ctx "ldir inside DJNZ loop mid-block rewinds to first iteration" {
	it::boot_cart [it::rom ldir_djnz_loop.rom]
	set bp1 [omsx::set_bp "0x4016"]
	set bp2 [omsx::set_bp "0x4019" {[reg BC] == 0x1000}]
	it::run_to_bp
	set time [omsx::machine_time]               ;# get time at 0x4016 (1st hit)
	it::cont_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x4019 "break fires on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x1000 "BC mid-block (0x1000)"
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg pc] 0x4019 "lands on LDIR"
	tcltest::eq_hex $ctx [it::reg bc] 0x2000 "BC restored to first iteration"
	tcltest::eq_hex $ctx [it::reg hl] 0x4000 "HL restored to source"
	tcltest::eq_hex $ctx [it::reg de] 0xc000 "DE restored to dest"
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg pc] 0x4016 "first time before LDIR"
	tcltest::eq $ctx [omsx::machine_time] $time "time when PC hits 0x4016 first time"
	it::cont_to_bp                              ;# go forward to 1st hit at 0x4019
	it::cont_to_bp                              ;# go forward to 2nd hit at 0x4016
	tcltest::eq_hex $ctx [it::reg pc] 0x4016 "second time before LDIR"
	set time [omsx::machine_time]               ;# get second time
	it::cont_to_bp                              ;# go forward to 2nd hit at 0x4019
	omsx::step_back
	omsx::step_back
	tcltest::eq $ctx [omsx::machine_time] $time "time when PC hits 0x4016 second time"
	it::cont_to_bp                              ;# go forward to 0x4019
	it::cont_to_bp                              ;# go forward to 3rd hit at 0x4016
	tcltest::eq_hex $ctx [it::reg pc] 0x4016 "third time before LDIR"
	set time [omsx::machine_time]               ;# get third time
	it::cont_to_bp                              ;# go forward to 0x4019
	omsx::step_back
	omsx::step_back
	tcltest::eq $ctx [omsx::machine_time] $time "time when PC hits 0x4016 third time"
	omsx::rm_bp $bp2
	omsx::rm_bp $bp1
	omsx::exit
}

# 5) Shared LDIR subroutine called with a different BC per call site
#    (BC=5, then 3, then 4; DI so no IRQ): breaking on the subroutine RET
#    after the THIRD execution and stepping back must land on the LDIR with
#    BC=4 -- the first iteration of the CURRENT execution, not an earlier
#    execution (BC=5 or BC=3).
tcltest::test $ctx "shared ldir subroutine varying BC rewinds current execution" {
	it::boot_cart [it::rom ldir_varying_bc.rom]
	set bp [omsx::set_bp "0x4029"]              ;# COPY's RET (BC=0 at each return)
	it::run_to_bp                               ;# 1st hit: end of BC=5 execution
	tcltest::eq_hex $ctx [it::reg pc] 0x4029 "first return reached"
	tcltest::eq_hex $ctx [it::reg de] 0xc005 "DE restored to first-execution dest"
	tcltest::eq_hex $ctx [it::reg bc] 0x0 "check end of execution: BC runs from 5 to 0"
	omsx::step_back
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg bc] 0x5 "first ldir (BC=5)"
	tcltest::eq_hex $ctx [it::reg pc] 0x4016 "first call reached"
	it::cont_to_bp                              ;# go forward again (1st hit)
	tcltest::eq_hex $ctx [it::reg pc] 0x4029 "first return reached again"
	omsx::step_in
	omsx::step_in
	tcltest::eq_hex $ctx [it::reg bc] 0x3 "second ldir (BC=3)"
	tcltest::eq_hex $ctx [it::reg pc] 0x401c "check if second call started"
	it::cont_to_bp                              ;# go forward (2nd hit)
	tcltest::eq_hex $ctx [it::reg pc] 0x4029 "second return reached"
	tcltest::eq_hex $ctx [it::reg de] 0xc008 "DE restored to second-execution dest"
	tcltest::eq_hex $ctx [it::reg bc] 0x0 "check end of execution: BC runs from 3 to 0"
	omsx::step_back
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg pc] 0x401c "second call reached"
	it::cont_to_bp                              ;# go forward again (2nd hit)
	tcltest::eq_hex $ctx [it::reg pc] 0x4029 "second return reached again"
	omsx::step_in
	omsx::step_in
	tcltest::eq_hex $ctx [it::reg bc] 0x4 "third ldir (BC=4)"
	tcltest::eq_hex $ctx [it::reg pc] 0x4022 "check if third call started"
	it::cont_to_bp                              ;# go forward (3rd hit)
	tcltest::eq_hex $ctx [it::reg pc] 0x4029 "third return reached"
	tcltest::eq_hex $ctx [it::reg de] 0xc00c "DE restored to third-execution dest"
	tcltest::eq_hex $ctx [it::reg bc] 0x0 "check end of execution: BC runs from 4 to 0"
	omsx::step_back
	omsx::step_back
	tcltest::eq_hex $ctx [it::reg pc] 0x4022 "third call reached"
	it::cont_to_bp                              ;# go forward again (3rd hit)
	tcltest::eq_hex $ctx [it::reg pc] 0x4029 "third return reached again"
	omsx::rm_bp $bp
	omsx::exit
}

# 6) Nested blocks: outer LDIR interrupted by video IRQs whose H.TIMI hook
#    runs its own INIR (B=4). Breaking after the outer block and stepping
#    back must land on the outer LDIR with BC=$2000 restored -- the inner
#    INIR runs (different PC) must not disturb it, and the outer resume must
#    not add a spurious outer entry (which would land mid-block).
tcltest::test $ctx "nested inir inside outer ldir rewinds outer to first iteration" {
	it::boot_cart [it::rom ldir_nested_inir.rom]
	set bp1 [omsx::set_bp "0x4028"]              ;# JR FOREVER (after outer LDIR)
	set bp2 [omsx::set_bp "0x4036"]              ;# POP HL (after inner INIR)
	it::run_to_bp
	for {set i 0} {$i < 10} {incr i} {
		if {[it::reg pc] == 0x4028} {
			omsx::step_back
			tcltest::eq_hex $ctx [it::reg pc] 0x4026 "$i: lands on outer LDIR"
			tcltest::eq_hex $ctx [it::reg bc] 0x2000 "$i: BC restored to 8192"
			tcltest::eq_hex $ctx [it::reg hl] 0x4000 "$i: HL restored to source"
			tcltest::eq_hex $ctx [it::reg de] 0xc000 "$i: DE restored to dest"
		} elseif {[it::reg pc] == 0x4036} {
			omsx::step_back
			tcltest::eq_hex $ctx [it::reg pc] 0x4034 "$i: step back to INIR block a fist time"
			tcltest::eq_hex $ctx [it::reg hl] 0xc100 "$i: HL restored to 0xc100"
			tcltest::eq_hex $ctx [it::reg bc] 0x0499 "$i: BC restored to 0x0499"
		}
		it::cont_to_bp
		it::cont_to_bp
	}
	omsx::rm_bp $bp2
	omsx::rm_bp $bp1
	omsx::exit
}

# 7) At a block's first iteration: calling step_back once lands on the block;
#    calling it again (now AT the block start) must fall back to a plain
#    single-instruction step back, not rewind to an earlier execution of the
#    same block PC.
tcltest::test $ctx "twice at block start steps one instruction back" {
	it::boot_cart [it::rom ldir_djnz_loop.rom]
	set bp1 [omsx::set_bp "0x4016"]              ;# LD BC,0x2000 right before LDIR
	set bp2 [omsx::set_bp "0x401b"]              ;# POP BC right after the LDIR
	it::run_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x4016 "break fires before block, first time"
	set time [omsx::machine_time]               ;# get time at 0x4016 (1st hit)
	it::cont_to_bp                              ;# run to 0x401b
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "break fires after block, first time"
	omsx::step_back
	omsx::step_back                             ;# back to 0x4016
	tcltest::eq $ctx [omsx::machine_time] $time "compare time when PC hits 0x4016, first time"
	it::cont_to_bp
	it::cont_to_bp                              ;# go to next iteraton of djnz
	tcltest::eq_hex $ctx [it::reg pc] 0x4016 "break fires before block, second time"
	set time [omsx::machine_time]               ;# get time at 0x4016 (2nd hit)
	it::cont_to_bp
	tcltest::eq_hex $ctx [it::reg pc] 0x401b "break fires after block, second time"
	omsx::step_back
	omsx::step_back                             ;# back to 0x4016
	tcltest::eq $ctx [omsx::machine_time] $time "compare time when PC hits 0x4016, second time"
	omsx::rm_bp $bp2
	omsx::rm_bp $bp1
	omsx::exit
}

# 8) Many pseudo-random LDIR sizes: the unrolled ROM runs 48 LDIRs whose BC
#    values were generated with a fixed seed and include 0 (65536 iterations),
#    single digits, and values up to $FFFF. Run the whole sequence once,
#    straight through to the 'JR FOREVER' after the last LDIR, then walk
#    backwards with step_back all the way to the first LDIR. Each step_back
#    from an after-point must rewind to that run's LDIR with BC restored;
#    each step_back from a block start must be a plain single-instruction
#    step onto the preceding 'LD BC,nn' (which is the after-point of the
#    previous LDIR).
tcltest::test $ctx "many random LDIR sizes rewind from last LDIR to first" {
	set rom [it::rom ldir_random_bc.rom]
	set entries [it::scan_ldirs $rom]
	tcltest::is $ctx {[llength $entries] >= 24} \
		"found dozens of LDIRs ([llength $entries])"

	it::boot_cart $rom

	# Pass 1: one uninterrupted run through all LDIRs. Stop on the first
	# instruction after the last LDIR ('JR FOREVER').
	set n [llength $entries]
	set last_ldir [lindex [lindex $entries end] 0]
	set bp [omsx::set_bp [format 0x%04X [expr {$last_ldir + 2}]]]
	it::run_to_bp
	omsx::rm_bp $bp

	# Pass 2: walk backwards one instruction at a time. From the instruction
	# after LDIR i, step_back rewinds to the start of that LDIR (BC restored).
	# From the LDIR start, the next step_back is a normal single-instruction
	# step onto the preceding 'LD BC,nn', which is the after-point of LDIR
	# i-1. Repeat down to entry 0.
	for {set i [expr {$n - 1}]} {$i >= 0} {incr i -1} {
		set e [lindex $entries $i]
		set ldir_addr [lindex $e 0]
		set bc [lindex $e 1]
		omsx::step_back
		tcltest::eq_hex $ctx [it::reg pc] $ldir_addr \
			"entry $i lands on its LDIR"
		tcltest::eq_hex $ctx [it::reg bc] $bc \
			"entry $i restores BC=[format 0x%04X $bc]"
		if {$i > 0} {
			omsx::step_back
			tcltest::eq_hex $ctx [it::reg pc] [expr {$ldir_addr - 3}] \
				"entry $i steps back onto its LD BC"
		}
	}
	omsx::exit
}

set fails [tcltest::summary $ctx]
if {$fails > 0} { exit 1 }
exit 0
