namespace eval codeanalyzer {

# TODO:
# * detect when interrupt routine is called;
# * finish all z80 instructions;
#   ** INI,IND,INIR,INDR,OUTI,OUTIR,OUTD,OUTDR
# * full BIOS support;
# * detect ROM size (16k, 32k or MEGAROM);
# * detect segment type (ROM or memory mapper) and usage when executing code or reading/writing data;
# * detect code copying to RAM and include it in the analysis;
# * detect instructon size using disassembler;
# * write assembly output to file;
# * annotate unscanned code/data when writing output file;
# * allow user to set markers on the code;
# * try to detect functions;
#   * detect call/ret combinations;
# * try to detect all types of RAM-to-VRAM memory copying:
#   * pattern generator table (PGT)
#   * name table;
#   * colour table;
#   * sprite generator table;
#   * sprite attribute table;
# * try to detect keyboard input;
# * try to detect joystick port input;
# * try to detect sound generation (PSG);

variable mem_type ""

variable t ;# memory type array
variable l ;# label array
variable c ;# comment array
variable x ;# extended address
variable b ;# bios label array
variable pc
variable tmp_pc {}
variable slot
variable segment
variable ss ""
variable is_mapper 0
variable r_wp {}
variable w_wp {}
variable start_point {}
variable entry_point {}
variable end_point 0xBFFF ;# end of page 2
variable comment "" ;# stored comment message
variable label_size 14
variable cmt_pos    19
variable error {} ;# error propagation

variable hkeyi_wp ;# HKEYI watchpoint
variable hkeyi {} ;# HKEYI routine address
variable intpc 0  ;# interrupted PC address

# bookkeeping
variable old_pc {}
variable last_mem_read {}

# info
variable DATA_recs 0 ;# data records
variable CODE_recs 0 ;# code records
variable BOTH_recs 0 ;# both DATA and CODE records

# constants
proc NAUGHT	{} { return {}}
proc CODE	{} { return 1 }
proc DATA	{} { return 2 }
proc BOTH	{} { return 3 }
proc HKEY_HOOK  {} { return 64924 } ;# last position of HKEY (0xfd9c)

set_help_proc codeanalyzer [namespace code codeanalyzer_help]
proc codeanalyzer_help {args} {
	if {[llength $args] == 1} {
		return {The codeanalyzer script creates annotated source code from dynamically analyzing running programs.
Recognized commands: start, stop, info, pixel
}
	}
	switch -- [lindex $args 1] {
		"start"	{ return {Start script that analyzes code.

Syntax: codeanalyzer start <slot> [<subslot>]

Analyze code from specified slot (0..3) and subslot (0..3).
}}
		"stop" { return {Stop script that analyzes code.

Syntax: codeanalyzer stop
}}
		"info" { return {Print info about code analysis to console.

Syntax: codeanalyzer info
}}
		"pixel" { return {Find piece of code that writes to screen positon (x, y).

Syntax: codeanalyzer pixel <x> <y>
}}
		"dump" { return {Dump source code to a file.

Syntax: codeanalyzer dump <filename>
}}
		"comment" { return {Comment on running code.

Syntax: codeanalyzer comment -data

Comment on the code as the program counter runs through it.
}}
		"labelsize" { return {Return current label length or change its value. Default value: 14

Syntax: codeanalyzer labelsize [<value>]

Comment on the code as the program counter runs through it.
}}
		default { error "Unknown command \"[lindex $args 1]\"."
}
	}
}

proc codeanalyzer_dispatch {args} {
	set params "[lrange $args 1 end]"
	switch -- [lindex $args 0] {
		"start"     { return [codeanalyzer_start {*}$params] }
		"stop"      { return [codeanalyzer_stop {*}$params] }
		"info"      { return [codeanalyzer_info {*}$params] }
		"comment"   { return [codeanalyzer_comment {*}$params] }
		"dump"      { return [codeanalyzer_dump {*}$params] }
		"labelsize" { return [codeanalyzer_labelsize {*}$params] }
		default     { error "Unknown command \"[lindex $args 0]\"." }
	}
}

proc codeanalyzer {args} {
	if {[env DEBUG] ne 0} {
		if {[catch {codeanalyzer_dispatch {*}$args} fid]} {
			puts stderr $::errorInfo
			error $::errorInfo
		}
	} else {
		codeanalyzer_dispatch {*}$args
	}
	return
}

proc slot {} {
	variable slot
	if {![info exists slot]} {
		error "no slot defined"
	}
	return [lindex $slot 0]
}

proc subslot {{defaults {}}} {
	variable slot
	if {![info exists slot]} {
		error "no slot defined"
	}
	if {[lindex $slot 1] eq {}} {
		return $defaults
	}
	return [lindex $slot 1]
}

# Get complete address in {slotted memory} format: [slot][subslot][64kb addr]
proc get_slotaddr {addr} {
	variable slot
	if {![info exists slot]} {
		error "no slot defined"
	}
	if {![llength slot] > 1} {
		return [expr ([slot] << 18) | ([subslot] << 16) | $addr]
	}
	return [expr ([slot] << 18) | $addr]
}

proc _get_selected_slot {page} {
	set ps_reg [debug read "ioports" 0xA8]
	set ps [expr {($ps_reg >> (2 * $page)) & 0x03}]
	if {[machine_info "issubslotted" $ps]} {
		set ss_reg [debug read "slotted memory" [expr {0x40000 * $ps + 0xFFFF}]]
		set ss [expr {(($ss_reg ^ 255) >> (2 * $page)) & 0x03}]
	} else {
		set ss 0
	}
	list $ps $ss
}

proc calc_addr {slot subslot addr} {
	if {[env DEBUG] ne 0 && ($slot eq {} || $subslot eq {} || $addr eq {})} { error "parameters missing" }
	return [expr ($slot << 18) | ($subslot << 16) | $addr]
}

# Get current address as used in {slotted memory} format: [current slot][current subslot][64KB addr]
proc get_curraddr {addr} {
	if {[env DEBUG] ne 0 && $addr eq {}} { error "address missing" }
	return [calc_addr {*}[list {*}[_get_selected_slot [expr $addr >> 14]] $addr]]
}

proc reset_info {} {
	variable mem_type ""
	variable t {}
	variable l {}
	variable c {}
	variable x {}
	variable b {}
	variable start_point {}
	variable entry_point {}
	variable DATA_recs 0
	variable CODE_recs 0
	variable BOTH_recs 0
}

proc codeanalyzer_start {args} {
	variable mem_type
	variable pc {}
	variable start_point
	variable entry_point
	variable r_wp
	variable w_wp
	variable is_mapper
	variable hkeyi_wp

	if {$args eq {} || [llength $args] > 2} {
		error "wrong # args: should be slot ?subslot?"
	}

	;# check slot subslot configuration
	set tmp [lrange $args 0 end]
	if {[machine_info issubslotted [lindex $tmp 0]]} {
		if {[llength $tmp] ne 2} {
			error "slot $slot is extended but subslot parameter is missing."
		}
	} elseif {[llength $tmp] ne 1} {
		error "slot is not extended but subslot is defined."
	}
	variable slot
	if {[info exists slot] && $slot ne $args} {
		puts "resetting entry point"
		reset_info
	}
	set slot $tmp
	set is_mapper [expr [get_mapper_size {*}[lrange [expr "{$slot 0}"] 0 2]] != 0]
	;# set breakpoints according to slot and subslot
	if {$r_wp eq ""} {
		puts "codeanalyzer started"
		load_bios
		if {$entry_point eq ""} {
			codeanalyzer_scancart
		}
		set r_wp     [debug set_watchpoint read_mem  {0x0000 0xffff} "\[pc_in_slot $slot\]" codeanalyzer::_read_mem   ]
		set w_wp     [debug set_watchpoint write_mem {0x0000 0xffff} "\[pc_in_slot $slot\]" codeanalyzer::_write_mem  ]
		set hkeyi_wp [debug set_watchpoint write_mem {0xfd9a 0xfd9c} {}                     codeanalyzer::_write_hkeyi]
		# detect BIOS call
		set bios_wp  [debug set_watchpoint read_mem  {0x0008 0x0159} "\[pc_in_slot 0\]"     codeanalyzer::_read_bios  ]
	} else {
		puts "Nothing to start."
	}

	return ;# no output
}

# detect write to HKEYI system hook
proc _write_hkeyi {} {
	if {$::wp_last_address eq [HKEY_HOOK]} {
		variable hkeyi
		set hkeyi [get_slotaddr [peek16 [expr $::wp_last_address - 1]]]
		log "HKEYI set to $hkeyi"
	}
}

proc codeanalyzer_stop {} {
	variable r_wp
	variable w_wp
	if {$r_wp ne ""} {
		puts "Codeanalyzer stopped."
		debug remove_watchpoint $r_wp
		debug remove_watchpoint $w_wp
		debug remove_watchpoint $hkeyi_wp
		set r_wp ""
		set w_wp ""
		set hkeyi_wp ""
	} else {
		puts "Nothing to stop."
	}
}

proc _scancart {} {
	variable mem_type
	variable ss
	variable slot
	variable start_point
	variable entry_point
	foreach offset [list 0x4000 0x8000 0x0000] { ;# memory search order
		set addr [get_slotaddr $offset]
		set tmp [peek16 $addr {slotted memory}]
		set prefix [format %c%c [expr $tmp & 0xff] [expr $tmp >> 8]]
		if {$prefix eq "AB"} {
			set mem_type ROM
			puts "prefix found at $ss:[xn [expr $addr & 0xffff]]"
			set start_point $offset
			set entry_point [peek16 [expr $addr + 2] {slotted memory}]
			puts "start point found at [xn $start_point]"
			puts "entry point found at [xn $entry_point]"
		}
	}
	if {$start_point eq ""} {
		puts "no cartridge signature found"
		set start_point ""
		set entry_point ""
	}
}

proc codeanalyzer_scancart {} {
	variable slot
	variable subslot

	if {![info exists slot]} {
		error "no slot defined"
	}
	variable ss [slot]
	if {[machine_info issubslotted [slot]]} {
		if {[subslot] eq ""} {
			error "no subslot defined"
		}
		append ss "-[subslot]"
	}

	_scancart
}

proc codeanalyzer_info {} {
	variable ss
	if {$ss eq ""} {
		error "codeanalyzer was never executed."
	}

	variable mem_type
	variable is_mapper
	variable start_point
	variable entry_point
	variable DATA_recs
	variable CODE_recs
	variable BOTH_recs
	variable r_wp
	variable hkeyi

	puts "running on slot $ss"
	puts "HKEYI set to $hkeyi"
	puts "mapper detection: [expr $is_mapper == 0 ? no : yes]"
	puts -nonewline "memory type: "
	if {$mem_type ne ""} {
		puts $mem_type
	} else {
		puts "???"
	}
	puts -nonewline "start point: "
	if {$start_point ne {}} {
		puts [xn $start_point]
	} else {
		puts "undefined"
	}
	puts -nonewline "entry point: "
	if {$entry_point ne {}} {
		puts [xn $entry_point]
	} else {
		puts "undefined"
	}
	puts "number of DATA records: $DATA_recs"
	puts "number of CODE records: $CODE_recs"
	puts "number of BOTH records: $BOTH_recs"

	puts -nonewline "codeanalyzer "
	if {$r_wp ne ""} {
		puts "still running"
	} else {
		puts "stopped"
	}
}

proc log {s} {
	if {[env DEBUG] ne 0} {
		puts stderr $s
		puts $s
	}
}

proc env {varname {defaults {}}} {
	if {[info exists ::env($varname)]} {
		return $::env($varname);
	}
	return $defaults;
}

proc tag_DATA {full_addr} {
	variable t
	variable DATA_recs
	variable CODE_recs
	variable BOTH_recs

	set type [array get t $full_addr]
	if {$type eq [NAUGHT]} {
		set t($full_addr) [DATA]
		if {[env LOGLEVEL] eq 1} {
			log "tagging [xn $full_addr] as DATA"
		}
		incr DATA_recs
	} elseif {$t($full_addr) eq [CODE]} {
		log "warning: overwritting address type in [xn $full_addr] from CODE to BOTH"
		set t($full_addr) [BOTH]
		incr CODE_recs -1
		incr BOTH_recs
	}
}

proc tag_CODE {full_addr} {
	variable t
	variable l
	variable DATA_recs
	variable CODE_recs
	variable BOTH_recs

	set type [array get t $full_addr]
	if {$type eq [NAUGHT]} {
		if {[env LOGLEVEL] eq 1} {
			log "tagging [xn $full_addr] as CODE"
		}
		set t($full_addr) [CODE]
		incr CODE_recs
	} elseif {$t($full_addr) eq [DATA]} {
		log "warning: overwritting address type in [xn $full_addr] from DATA to BOTH"
		set t($full_addr) [BOTH]
		incr DATA_recs -1
		incr BOTH_recs
	}
}

proc comment {full_addr comment} {
	variable c
	set curcmt [array get c $full_addr]
	if {[llength $curcmt] eq 0} {
		set c($full_addr) $comment
	} else {
		if {[string first $comment $c($full_addr)] eq -1} {
			log "append comment at $curcmt: $comment"
			set c($full_addr) "[lindex $curcmt 1], $comment"
		}
	}
}

proc labelfy {full_addr} {
	return "L_[format %04X [expr $full_addr & 0xffff]]"
}

# Find label for address or create a new one in the format "L_"<hex address>
proc tag_address {full_addr {name {}}} {
	variable b
	variable l
	if {$name ne {}} {
		set l($full_addr) $name
		return
	}
	# ignore BIOS address
	set lbl [array get b $full_addr]
	if {[llength $lbl] ne 0} {
		return
	}
	# tag CODE address
	set lbl [array get l $full_addr]
	if {[llength $lbl] eq 0} {
		# look for symbol in symbol table
		set syms [debug symbols lookup -value [expr 0xffff & $full_addr]]
		if {[llength $syms] > 0} {
			set sym [lindex $syms 0]
			set name [lindex $sym 3]
			log "[xn $full_addr]: found symbol $name"
			set l($full_addr) $name
		} elseif {$lbl eq [NAUGHT]} {
			set name [labelfy $full_addr]
			log "[xn $full_addr]: address tagged with new name $name."
			set l($full_addr) $name
		}
	}
}

# tag PC with extra information on memory access
proc tag_extra {full_pc full_addr {tmp {}}} {
	variable x
	if {[array get x $full_pc] eq {}} {
		log "tag slot and subslot for [xn $full_pc]: [xn $full_addr], $tmp"
		set x($full_pc) $full_addr
	}
}

# create label from lookup (uses global tmp_pc)
proc lookup_or_create {addr} {
	variable b
	variable l
	variable c
	variable tmp_pc
	if {[env DEBUG] ne 0 &&   $addr eq {}} { error "missing parameter addr" }
	if {[env DEBUG] ne 0 && $tmp_pc eq {}} { error "missing parameter tmp_pc" }

	set full_addr [get_curraddr $addr]
	tag_extra $tmp_pc $full_addr 1
	# return BIOS address only
	set lbl [array get b $full_addr]
	if {[llength $lbl] ne 0 && [array get c $tmp_pc] eq {}} {
		comment $tmp_pc "BIOS access detected"
		return [lindex $lbl 1]
	}
	# tag CODE address
	set lbl [array get l $full_addr]
	if {[llength $lbl] eq 0} {
		tag_address $full_addr
	}
	return [lindex $lbl 1]
}

proc disasm_pc {full_pc lookup} {
	if {[env DEBUG] ne 0 && $full_pc eq {}} { error "missing parameter full_pc" }
	set    blob [format %c [peek $full_pc {slotted memory}]]
	append blob [format %c [peek [expr $full_pc + 1] {slotted memory}]]
	append blob [format %c [peek [expr $full_pc + 2] {slotted memory}]]
	append blob [format %c [peek [expr $full_pc + 3] {slotted memory}]]
	return [disasm $blob $full_pc $lookup]
}

proc disasm {blob full_pc lookup} {
	variable tmp_pc
	variable error
	set tmp_pc $full_pc
	set error {}
	if {[catch {set result [debug disasm_blob $blob $full_pc $lookup]} fid]} {
		comment $full_pc $fid
		set result "db     "
		for {set i 0} {$i < [string length $blob]} {incr i} {
			append result "#[format %02x [scan [string index $blob $i] %c]],"
		}
		set result [list [string trimright $result ,] 4]
	}
	if {$error ne {}} {
		error $error
	}
	return $result
}

proc tag_decoded {full_pc lookup} {
	set len [lindex [disasm_pc $full_pc $lookup] 1]
	for {set i 0} {$i < $len} {incr i} {
		tag_CODE [expr $full_pc + $i]
	}
}

# Detect functions, branches, BIOS calls etc.
proc analyze_opcode {full_pc} {
	variable x
	set peekpc [peek $full_pc {slotted memory}]

	# already visited?
	if {[array get x $full_pc] ne {}} {
		return
	}
	if {$peekpc eq 24} {
		# tag unconditional relative branches as CODE
		log "analyze_opcode started: unconditional relative branch detected at [xn $full_pc]"
		set dest [expr $full_pc + [peek_s8 [expr $full_pc + 1] {slotted memory}] + 2]
		tag_extra $full_pc $dest 2
		tag_address $dest

	} elseif {[lsearch -exact [list 195 205] $peekpc] >= 0} {
		# tag unconditional absolute branches as CODE
		log "analyze_opcode started: absolute branch detected at [xn $full_pc]"
		set dest [get_curraddr [peek16 [expr $full_pc + 1] {slotted memory}]]
		tag_extra $full_pc $dest 0
		tag_address $dest

	} elseif {[lsearch -exact [list 16 32 40 48 56] $peekpc] >= 0} {
		# tag conditional relative branches as CODE
		log "analyze_opcode started: relative conditional branch detected at [xn $full_pc]"
		set dest [expr $full_pc + [peek_s8 [expr $full_pc + 1] {slotted memory}] + 2]
		tag_extra $full_pc $dest 2
		tag_address $dest
		tag_decoded $dest lookup_or_create
		set next [expr $full_pc + 2] ;# next instruction in adjacent memory
		tag_decoded $next lookup_or_create

	} elseif {[lsearch -exact [list 192 200 208 216 224 232 240 248] $peekpc] >= 0} {
		# tag conditional relative returns as CODE
		log "analyze_opcode started: relative conditional return detected at [xn $full_pc]"
		set next [expr $full_pc + 1] ;# next instruction in adjacent memory
		tag_decoded $next lookup_or_create

	} elseif {[lsearch -exact [list 194 196 202 204 210 212 218 220 226 228 234 236 242 244 250 252] $peekpc] >= 0} {
		# tag conditional absolute branches as CODE
		log "analyze_opcode started: absolute conditional branch detected at [xn $full_pc]"
		set dest [get_curraddr [peek16 [expr $full_pc + 1] {slotted memory}]]
		tag_extra $full_pc $dest 3
		tag_address $dest
		tag_decoded $dest lookup_or_create
		set next [expr $full_pc + 3] ;# next instruction in adjacent memory
		tag_decoded $next lookup_or_create
	}
	#tag_extra $full_pc $full_pc 4
	#log "analyze_opcode done"
}

# hex or "null"
proc xn {num {i 4}} {
	if {$num eq ""} {
		return "ø"
	}
	return [format "%0${i}x" $num]
}

# pattern name table location and size
proc read_pnt {} {
	set mode [get_screen_mode]
	if {$mode eq 1} {
		return [list [expr ([vdpreg 2] & 0b01111111) * 0x400] 256 PNT]
	} elseif {$mode eq 2} {
		return [list [expr ([vdpreg 2] & 0b01111111) * 0x400] 768 PNT]
	} elseif {$mode eq 4} {
		return [list [expr ([vdpreg 2] & 0b11111111) * 0x400] 1024 PNT]
	}
	# out of reach
	return [list 0x20000 0 PNT]
}

# colour table location and size
proc read_ct {} {
	set mode [get_screen_mode]
	if {$mode eq 2} {
		return [list [expr ([vdpreg 3] & 0b10000000) * 0x800] 6144 CT]
	} elseif {$mode eq 4} {
		return [list [expr ([vdpreg 3] * 0x800] 8192 CT]
	}
	# out of reach
	return [list 0x20000 0 CT]
}

# pattern generator table location and size
proc read_pgt {} {
	set mode [get_screen_mode]
	if {$mode eq 2} {
		return [list [expr ([vdpreg 4] & 0b00000100) * 0x800] 6144 PGT]
	} elseif {$mode eq 4} {
		return [list [expr ([vdpreg 4] & 0b00111111) * 0x800] 8192 PGT]
	}
	# out of reach
	return [list 0x20000 0 PGT]
}

# sprite attribute table location and size
proc read_sat {} {
	set mode [get_screen_mode]
	if {$mode > 3} {
		return [list [expr ((([vdpreg 5] | 0b00000100) & 0b11111100) + (([vdpreg 11] & 0b00000011) << 8)) * 0x80] 128 SAT]
	} elseif {$mode > 0} {
		return [list [expr ([vdpreg 5] & 0b01111111) * 0x80] 128 SAT]
	}
	# out of reach
	return [list 0x20000 0 SAT]
}

# sprite pattern table location and size
proc read_spt {} {
	set mode [get_screen_mode]
	if {$mode > 0} {
		return [list [expr ([vdpreg 6] & 0b00000111) * 0x800] 2048 SPT]
	} elseif {$mode > 3} {
		return [list [expr ([vdpreg 6] & 0b00111111) * 0x800] 2048 SPT]
	}
	# out of reach
	return [list 0x20000 0 SPT]
}

# sprite colour table location and size
proc read_sct {} {
	set mode [get_screen_mode]
	if {$mode > 3} {
		return [list [expr ([vdpreg 6] & 0b00111111) * 0x800 - 512] 512 SCT]
	}
	# out of reach
	return [list 0x20000 0 SCT]
}

proc find_table_vaddr {vaddr} {
	if {[get_screen_mode] < 2} { return {} }
	set regions [list [read_pnt] [read_ct] [read_pgt] [read_sat] [read_spt] [read_sct]]
	set tables {}
	foreach r $regions {
		set start [lindex $r 0]
		if {$vaddr < $start} { continue }
		if {$vaddr > [expr $start + [lindex $r 1] - 1]} { continue }
		lappend tables [lindex $r 2]
	}
	return $tables
}

proc _read_bios {} {
	variable r_wp
	if {$r_wp eq ""} { error "codeanalyzer not running yet" }
	if {$::wp_last_address ne [reg PC]} { return }

	variable x
	variable old_pc
	if {$old_pc eq {}} { return }
	set full_pc [get_curraddr $old_pc]
	if {[array get x $full_pc] ne {}} { return }

	variable b
	set bcall [array get b $::wp_last_address]
	if {$bcall ne {}} { log "analyzing BIOS call at [xn $old_pc]... [lindex $bcall 1]" }

	switch [lindex $bcall 1] {
		RDVDP { ;# read VDP status register
			set reg [reg A]
			comment $full_pc "reading vdp register"
		}
		WRTVDP { ;# write VDP status register
			set byte [reg B]
			set reg  [reg C]
			comment $full_pc "writing byte to vdp register"
		}
		RDVRM { ;# read data to VRAM
			set vaddr [reg HL]
			comment $full_pc "reading VRAM address in [find_table_vaddr $vaddr]"
		}
		WRTVRM { ;# write data to VRAM
			set vaddr [reg HL]
			set byte  [reg A]
			comment $full_pc "writing byte to VRAM address in [find_table_vaddr $vaddr]"
		}
		FILVRM { ;# fill VRAM
			set byte  [reg A]
			set len   [reg BC]
			set vaddr [reg HL]
			comment $full_pc "filling vram address in [find_table_vaddr $vaddr]"
		}
		LDIRVM { ;# from RAM to VRAM
			set len   [reg BC]
			set vaddr [reg DE]
			set addr  [reg HL]
			comment $full_pc "writing bytes from RAM to VRAM in [find_table_vaddr $vaddr]"
		}
		LDIRMV { ;# from VRAM to RAM
			set len   [reg BC]
			set addr  [reg DE]
			set vaddr [reg HL]
			comment $full_pc "writing bytes to RAM from VRAM address in [find_table_vaddr $vaddr]"
		}
		SETRD  { ;# set VDP address to read
			set vaddr [reg HL]
			comment $full_pc "setting VDP to read"
		}
		SETWRT { ;# set VDP address to write
			set vaddr [reg HL]
			comment $full_pc "setting VDP to write"
		}
		SNSMAT { ;# read keyboard matrix
			set line [reg A]
			comment $full_pc "reading keyboard matrix"
		}
	}
}

proc _read_mem {} {
	variable t
	variable x
	variable old_pc
	variable last_mem_read
	variable comment
	variable hkeyi
	variable intpc

	set full_pc [get_curraddr [reg PC]]
	if {$full_pc eq $hkeyi && $intpc eq 0} {
		log "interrupt routine detected"
		set intpc $old_pc
	} elseif {[reg PC] eq $intpc} {
		log "resuming from interrupt routine detected"
		set intpc 0
	}

	#set int [expr $intpc == 0 ? 0 : 1]
	#log "pc: [xn $old_pc] ([xn $last_mem_read]) -> [xn $full_pc], ([xn $::wp_last_address]), intpc$int"

	# process current instruction
	if {$old_pc eq [reg PC]} {
		# detect if ::wp_last_address is still reading instructions
		if {$::wp_last_address eq [expr $last_mem_read + 1]} {
			tag_CODE [get_curraddr $::wp_last_address]
		} elseif {$::wp_last_address ne [expr [reg PC]]} {
			set full_addr [get_curraddr $::wp_last_address]
			tag_extra $full_pc $full_addr 5
			tag_address $full_addr
			tag_DATA $full_addr
		}
	} else {
		# analyze last instruction
		if {$old_pc ne {}} { analyze_opcode [get_curraddr $old_pc] }
		# start new instruction
		tag_CODE [get_curraddr [reg PC]]
		# put comment on new instruction if it exists
		if {$comment ne {}} { comment $full_pc $comment }
	}
	set old_pc [reg PC]
	set last_mem_read $::wp_last_address
}

proc _write_mem {} {
	tag_DATA [get_curraddr $::wp_last_address]
}

proc label_fmt {label} {
	variable label_size
	if {$label ne ""} { set label $label: }
	set size [expr $label_size - [string len $label]]
	if {$size < 0} {
		return "$label\n[string repeat "Y" $label_size]"
	}
	return $label[string repeat " " $size]
}

proc disasm_fmt {label asm {comment ""}} {
	variable cmt_pos
	if {$comment eq ""} {
		set suffix  [format %${cmt_pos}s $asm]
	} else {
		set suffix "[format %${cmt_pos}s $asm] ; $comment"
	}
	set line [label_fmt $label]$suffix
	return $line
}

proc lookup {addr} {
	variable b
	variable l
	variable x
	variable tmp_pc
	variable error
	if {[env DEBUG] ne 0 && $tmp_pc eq {}} { error "missing parameter tmp_pc \[1\]" }

	#log "\[1\] searching [xn $tmp_pc] [xn $addr]..."
	# search extended address information
	if {[array get x $tmp_pc] ne {}} {
		set full_addr $x($tmp_pc)
		if {[expr $full_addr & 0xffff] ne $addr} {
			set error "error: [xn $tmp_pc]: [xn $full_addr] != [xn $addr]"
			return
		}
	} else {
		set full_addr [get_curraddr $addr]
		#error "extended address at [xn $tmp_pc] not found"
		#log "error: [xn $full_addr] != [xn $addr]"
	}
	#log "\[2\] searching [xn $addr]..."
	# return BIOS address
	set lbl [array get b $full_addr]
	if {[llength $lbl] ne 0} {
		variable c
		#log "found BIOS entry @[xn $full_addr]: [lindex $lbl 1]"
		if {[array get c $tmp_pc] eq {}} {
			comment $tmp_pc "BIOS access detected"
		}
		return [lindex $lbl 1]
	}
	# return CODE address
	set lbl [array get l $full_addr]
	if {[llength $lbl] ne 0} {
		#log "found @[xn $full_addr]: [lindex $lbl 1]"
		return [lindex $lbl 1]
	}
	return ""
}

proc disasm_to_file {source_file full_addr blob {use_disasm 1}} {
	variable l
	variable c
	variable comment

	while {[string length $blob] > 0} {
		if {$use_disasm} {
			set asm [disasm $blob $full_addr lookup]
		} else {
			set byte [scan $blob %c]
			set asm [list "db     #[format %02x $byte]         " 1]
		}
		# remove processed data from blob
		set blob [string range $blob [lindex $asm 1] end]
		set lbl  [array get l $full_addr]
		set cmt  [array get c $full_addr]

		if {$lbl ne {}} { set lbl $l($full_addr) }
		if {$cmt ne {}} { set cmt $c($full_addr) }
		puts $source_file [disasm_fmt $lbl [lindex $asm 0] $cmt]
		incr full_addr [lindex $asm 1]
	}
}

proc dump_mem {source_file full_addr} {
	disasm_to_file $source_file $full_addr [format %c [peek $full_addr {slotted memory}]] 0
}

proc dump_blob {source_file full_addr blob} {
	if {$blob ne ""} {
		disasm_to_file $source_file $full_addr $blob
	}
	return ""
}

proc codeanalyzer_comment {message} {
	puts "comment called with comment \"$message\"."
	variable comment
	set comment $message
}

proc codeanalyzer_dump {{filename "./source.asm"}} {
	variable t
	variable start_point
	variable entry_point
	variable end_point
	set source_file [open $filename {WRONLY TRUNC CREAT}]
	set blob ""
	set start_addr {}

	if {$start_point eq {}} {
		error "unknown program entry point"
	}
	tag_address [get_slotaddr $start_point] "START"
	tag_address [get_slotaddr $entry_point] "MAIN"
	for {set offset $start_point} {$offset < $end_point} {incr offset} {
		set addr [get_slotaddr $offset]
		if {[array get t $addr] ne {}} {
			set type $t($addr)
			if {$type eq [CODE]} {
				if {$blob eq ""} {
					set start_addr $addr
				}
				append blob [format %c [peek $addr {slotted memory}]]
			} else {
				# end of blob
				dump_blob $source_file $start_addr $blob
				dump_mem $source_file $addr
				set blob ""
			}
		} else {
			dump_blob $source_file $start_addr $blob
			dump_mem $source_file $addr
			set blob ""
		}
	}
	if {$blob ne ""} {
		dump_blob $source_file $start_addr $blob
	}
	close $source_file
}

proc codeanalyzer_labelsize {{value {}}} {
	variable label_size
	if {$value eq {}} {
		return $label_size
	}
	set label_size $value
}

proc load_bios {} {
	variable b
	set b([calc_addr 0 0 0x0006]) vdp.dr
	set b([calc_addr 0 0 0x0007]) vdp.dw
	set b([calc_addr 0 0 0x0008]) SYNCHR
	set b([calc_addr 0 0 0x000c]) RDSLT
	set b([calc_addr 0 0 0x0010]) CHRGTR
	set b([calc_addr 0 0 0x0014]) WRSLT
	set b([calc_addr 0 0 0x0018]) OUTDO
	set b([calc_addr 0 0 0x001c]) CALSLT
	set b([calc_addr 0 0 0x0020]) DCOMPR
	set b([calc_addr 0 0 0x0024]) ENASLT
	set b([calc_addr 0 0 0x0024]) ENASLT
	set b([calc_addr 0 0 0x0028]) GETYPR
	set b([calc_addr 0 0 0x0030]) CALLF
	set b([calc_addr 0 0 0x0038]) KEYINT
	set b([calc_addr 0 0 0x003b]) INITIO
	set b([calc_addr 0 0 0x003e]) INIFNK
	set b([calc_addr 0 0 0x0041]) DISSCR
	set b([calc_addr 0 0 0x0044]) ENASCR
	set b([calc_addr 0 0 0x0047]) WRTVDP
	set b([calc_addr 0 0 0x004a]) RDVRM
	set b([calc_addr 0 0 0x004d]) WRTVRM
	set b([calc_addr 0 0 0x0050]) SETRD
	set b([calc_addr 0 0 0x0053]) SETWRT
	set b([calc_addr 0 0 0x0056]) FILVRM
	set b([calc_addr 0 0 0x0059]) LDIRMV
	set b([calc_addr 0 0 0x005c]) LDIRVM
	set b([calc_addr 0 0 0x005f]) CHGMOD
	set b([calc_addr 0 0 0x0062]) CHGCLR
	set b([calc_addr 0 0 0x0066]) NMI
	set b([calc_addr 0 0 0x0069]) CLRSPR
	set b([calc_addr 0 0 0x006c]) INITXT
	set b([calc_addr 0 0 0x006f]) INIT32
	set b([calc_addr 0 0 0x006f]) INIGRP
	set b([calc_addr 0 0 0x0075]) INIMLT
	set b([calc_addr 0 0 0x0078]) SETTXT
	set b([calc_addr 0 0 0x007b]) SETT32
	set b([calc_addr 0 0 0x007e]) SETGRP
	set b([calc_addr 0 0 0x0081]) SETMLT
	set b([calc_addr 0 0 0x0084]) CALPAT
	set b([calc_addr 0 0 0x0087]) CALATR
	set b([calc_addr 0 0 0x008a]) GSPSIZ
	set b([calc_addr 0 0 0x008d]) GSPPRT
	set b([calc_addr 0 0 0x0090]) GICINI
	set b([calc_addr 0 0 0x0093]) WRTPSG
	set b([calc_addr 0 0 0x0096]) RDPSG
	set b([calc_addr 0 0 0x0099]) STRTMS
	set b([calc_addr 0 0 0x009c]) CHSNS
	set b([calc_addr 0 0 0x009f]) CHGET
	set b([calc_addr 0 0 0x00a2]) CHPUT
	set b([calc_addr 0 0 0x00a5]) LPTOUT
	set b([calc_addr 0 0 0x00a8]) LPTSTT
	set b([calc_addr 0 0 0x00ab]) CNVCHR
	set b([calc_addr 0 0 0x00ae]) PINLIN
	set b([calc_addr 0 0 0x00b1]) INLIN
	set b([calc_addr 0 0 0x00b4]) QINLIN
	set b([calc_addr 0 0 0x00b7]) BREAKX
	set b([calc_addr 0 0 0x00ba]) ISCNTC
	set b([calc_addr 0 0 0x00bd]) CKCNTC
	set b([calc_addr 0 0 0x00c0]) BEEP
	set b([calc_addr 0 0 0x00c3]) CLS
	set b([calc_addr 0 0 0x00c6]) POSIT
	set b([calc_addr 0 0 0x00c9]) FNKSB
	set b([calc_addr 0 0 0x00cc]) ERAFNK
	set b([calc_addr 0 0 0x00cf]) DSPFNK
	set b([calc_addr 0 0 0x00d2]) TOTEXT
	set b([calc_addr 0 0 0x00d5]) GTSTCK
	set b([calc_addr 0 0 0x00d8]) GTTRIG
	set b([calc_addr 0 0 0x00db]) GTPAD
	set b([calc_addr 0 0 0x00de]) GTPDL
	set b([calc_addr 0 0 0x00e1]) TAPION
	set b([calc_addr 0 0 0x00e4]) TAPIN
	set b([calc_addr 0 0 0x00e7]) TAPIOF
	set b([calc_addr 0 0 0x00ea]) TAPOON
	set b([calc_addr 0 0 0x00ed]) TAPOUT
	set b([calc_addr 0 0 0x00f0]) TAPOOF
	set b([calc_addr 0 0 0x00f3]) STMOTR
	set b([calc_addr 0 0 0x00f6]) LFTQ
	set b([calc_addr 0 0 0x00f9]) PUTQ
	set b([calc_addr 0 0 0x00fc]) RIGHTC
	set b([calc_addr 0 0 0x00ff]) LEFTC
	set b([calc_addr 0 0 0x0102]) UPC
	set b([calc_addr 0 0 0x0105]) TUPC
	set b([calc_addr 0 0 0x0108]) DOWNC
	set b([calc_addr 0 0 0x010b]) TDOWNC
	set b([calc_addr 0 0 0x010e]) SCALXY
	set b([calc_addr 0 0 0x0111]) MAPXY
	set b([calc_addr 0 0 0x0114]) FETCH
	set b([calc_addr 0 0 0x0117]) STOREC
	set b([calc_addr 0 0 0x011a]) SETATR
	set b([calc_addr 0 0 0x011d]) READC
	set b([calc_addr 0 0 0x0120]) SETC
	set b([calc_addr 0 0 0x0123]) NSETCX
	set b([calc_addr 0 0 0x0126]) GTASPC
	set b([calc_addr 0 0 0x0129]) PNTINI
	set b([calc_addr 0 0 0x012c]) SCANR
	set b([calc_addr 0 0 0x012f]) SCANL
	set b([calc_addr 0 0 0x0132]) CHGCAP
	set b([calc_addr 0 0 0x0135]) CHGSND
	set b([calc_addr 0 0 0x0138]) RSLREG
	set b([calc_addr 0 0 0x013b]) WSLREG
	set b([calc_addr 0 0 0x013e]) RDVDP
	set b([calc_addr 0 0 0x0141]) SNSMAT
	set b([calc_addr 0 0 0x0144]) PHYDIO
	set b([calc_addr 0 0 0x0147]) FORMAT
	set b([calc_addr 0 0 0x014a]) ISFLIO
	set b([calc_addr 0 0 0x014d]) OUTDLP
	set b([calc_addr 0 0 0x0150]) GETVCP
	set b([calc_addr 0 0 0x0153]) GETVC2
	set b([calc_addr 0 0 0x0156]) KILBUF
	set b([calc_addr 0 0 0x0159]) CALBAS
	set b([calc_addr 3 0 0xfc4a]) HIMEM@1
	set b([calc_addr 3 0 0xfc4b]) HIMEM@2
	set b([calc_addr 3 0 0xfcc1]) EXPTBL@1
	set b([calc_addr 3 0 0xfcc2]) EXPTBL@2
	set b([calc_addr 3 0 0xfcc3]) EXPTBL@3
	set b([calc_addr 3 0 0xfcc4]) EXPTBL@4
	set b([calc_addr 3 0 0xfd9a]) HKEYI@1
	set b([calc_addr 3 0 0xfd9b]) HKEYI@2
	set b([calc_addr 3 0 0xfd9c]) HKEYI@3
}

namespace export codeanalyzer

} ;# namespace codeanalyzer

namespace import codeanalyzer::*
