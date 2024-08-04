namespace eval codeanalyzer {

;# TODO:
;# * try to detect functions;
;# * try to detect all types of RAM-to-VRAM memory copying:
;#   ** pattern generator table (PGT)
;#   ** name table;
;#   ** colour table;
;#   ** sprite generator table;
;#   ** sprite attribute table;
;# * try to detect sound generation (PSG);

variable m ;# memory
variable pc
variable slot
variable segment
variable cond {}

;# constants
proc NAUGHT		{} { return {}}
proc CODE		{} { return 1 }
proc DATA		{} { return 2 }
proc EDITABLE_CODE	{} { return 3 }

set_help_proc codeanalyzer [namespace code codeanalyzer_help]
proc codeanalyzer_help {args} {
	if {[llength $args] == 1} {
		return {The codeanalyzer script creates annotated source code from dynamically analyzing running programs.
Recognized commands: start, stop, pixel
}
	}
	switch -- [lindex $args 1] {
		"start"	{ return {Start script to analyze code.

Syntax: codeanalyzer start <slot> [<subslot>]

Analyze code from specified slot (0..3) and subslot (0..3).
}}
		"stop"  { return {Stop script that analyzes code.
}}
		"pixel" { return {Find piece of code that writes to screen positon (x, y).

Syntax: pixel <x> <y>
}}
		default { error "Unknown command \"[lindex $args 1]\""
}
	}
}

proc codeanalyzer {args} {
	if {[lindex $args 0] eq "start"} {
		return [codeanalyzer_start [lrange $args 1 end]]
	}
	if {[lindex $args 0] eq "stop"} {
		return [codeanalyzer_stop]
	}
}

proc codeanalyzer_start {args} {
	variable pc {}

	if {[lindex $args] < 1} {
		error "wrong # args: should be slot subslot"
	}

	variable slot    [lindex $args 0]
	variable subslot [lindex $args 1]

	;# set condition according to slot and subslot
	if {[info exists ::env(DEBUG)] && $::env(DEBUG) eq 1} {
		set cond [debug set_condition "\[pc_in_slot $slot\]" codeanalyzer::_debugmem]
	} else {
		set cond [debug set_condition "\[pc_in_slot $slot\]" codeanalyzer::_checkmem]
	}
	return
}

proc codeanalyzer_stop {} {
	debug remove_condition $cond
}

proc _debugmem {} {
	if {[catch {_checkmem} fid]} {
		puts stderr "fid; $fid"
		puts stderr $::errorInfo
		debug break 
	}
}

proc log {s} {
	if {[info exists ::env(DEBUG)] && $::env(DEBUG) eq 1} {
		puts stderr $s
	}
}

proc markdata {addr} {
	variable m
	set type [array get m $addr]

	if {$type eq [NAUGHT]} {
		set m($addr) [DATA]
		log "marking [format %04x $addr] as DATA"
	} elseif {$m($addr) eq [CODE]} {
		log "warning: overwritting address type in [format %04x $addr] from CODE to EDITABLE_CODE"
		set m($addr) [EDITABLE_CODE]
	}
}

proc markcode {addr} {
	variable m
	set type [array get m $addr]

	if {$type eq [NAUGHT]} {
		set m($addr) [CODE]
		log "marking [format %04x $addr] as CODE"
	} elseif {$m($addr) eq [DATA]} {
		log "warning: overwritting address type in [format %04x $addr] from DATA to EDITABLE_CODE"
		set m($addr) [EDITABLE_CODE]
	}
}

proc _checkmem {} {
	variable m
	variable pc [reg PC]
	variable hl [reg HL]
	variable bc [reg BC]
	variable de [reg DE]
	variable ix [reg IX]
	variable iy [reg IY]
	variable p1 [expr $pc + 1]
	variable p2 [expr $pc + 2]
	variable p3 [expr $pc + 3]

	;# CODE memory
	if {[array get m $pc] eq ""} {
		markcode $pc
	}
	;# DATA memory
	switch -- [format %02x [peek $pc]] {
		02 { ;# LD (BC),A
			markdata $bc
		}
		12 { ;# LD (DE),A
			markdata $de
		}
		32 { ;# LD (word),A
			set word [expr [peek $p1] + ([peek $p2] << 8)]
			markdata $word
		}
		7e { ;# LD A,(HL)
			markdata $hl
		}
		46 { ;# LD B,(HL)
			markdata $hl
		}
		4e { ;# LD C,(HL)
			markdata $hl
		}
		56 { ;# LD D,(HL)
			markdata $hl
		}
		5e { ;# LD E,(HL)
			markdata $hl
		}
		66 { ;# LD H,(HL)
			markdata $hl
		}
		6e { ;# LD L,(HL)
			markdata $hl
		}
		77 { ;# LD (HL),A
			markdata $hl
		}
		70 { ;# LD (HL),B
			markdata $hl
		}
		71 { ;# LD (HL),C
			markdata $hl
		}
		72 { ;# LD (HL),D
			markdata $hl
		}
		73 { ;# LD (HL),E
			markdata $hl
		}
		74 { ;# LD (HL),H
			markdata $hl
		}
		75 { ;# LD (HL),L
			markdata $hl
		}
		0a { ;# LD A,(BC)
			markdata $bc
		}
		1a { ;# LD A,(DE)
			markdata $de
		}
		2a { ;# LD HL,(word)
			set word [expr [peek $p1] | ([peek $p2] << 8)]
			markdata $word
		}
		34 { ;# INC (HL)
			markdata $hl
		}
		35 { ;# DEC (HL)
			markdata $hl
		}
		36 { ;# LD (HL),byte
			markdata $hl
		}
		3a { ;# LD A,(word) # 0x3a
			set word [expr [peek $p1] | ([peek $p2] << 8)]
			markdata $word
		}
		86 { ;# ADD A, (HL)
			markdata $hl
		}
		8e { ;# ADC A, (HL)
			markdata $hl
		}
		96 { ;# SUB A, (HL)
			markdata $hl
		}
		9e { ;# SBC A, (HL)
			markdata $hl
		}
		a6 { ;# AND (HL)
			markdata $hl
		}
		ae { ;# XOR (HL)
			markdata $hl
		}
		b6 { ;# OR (HL)
			markdata $hl
		}
		be { ;# CP (HL)
			markdata $hl
		}
		cb { ;# rotations with (HL)
			if {[peek $p1] == 0x06} { ;# RLC (HL)
				markdata $hl
			}
			elseif {[peek $p1] == 0x0e} { ;# RRC (HL)
				markdata $hl
			}
			elseif {[peek $p1] == 0x16} { ;# RL (HL)
				markdata $hl
			}
			elseif {[peek $p1] == 0x1e} { ;# RL (HL)
				markdata $hl
			}
		}
		ed {
			;# LD XX, (word)
			if {[peek $p1] == 0x4b} { ;# XX=BC
				set word [expr [peek $p2] + ([peek $p3] << 8)]
				markdata $word
			}
			elseif {[peek $p1] == 0x5b} { ;# XX=DE
				set word [expr [peek $p2] + ([peek $p3] << 8)]
				markdata $word
			}
			elseif {[peek $p1] == 0x6b} { ;# XX=HL
				set word [expr [peek $p2] + ([peek $p3] << 8)]
				markdata $word
			}
			elseif {[peek $p1] == 0x7b} { ;# XX=SP
				set word [expr [peek $p2] + ([peek $p3] << 8)]
				markdata $word
			}
			;# CP operations with (HL)
			elseif {[peek $p1] == 0xa1} { ;# CPI
				markdata $hl
			}
			elseif {[peek $p1] == 0xb1} { ;# CPIR
				markdata $hl
			}
			elseif {[peek $p1] == 0xa9} { ;# CPD
				markdata $hl
			}
			elseif {[peek $p1] == 0xb9} { ;# CPDR
				markdata $hl
			}
		}
		dd { ;# operation with IX somewhere
			;# LD X,(IX + index)
			if {[peek $p1] == 0x7e} { ;# X=A
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
			elseif {[peek $p1] == 0x46} { ;# X=B
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
			elseif {[peek $p1] == 0x4e} { ;# X=C
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
			elseif {[peek $p1] == 0x56} { ;# X=D
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
			elseif {[peek $p1] == 0x5e} { ;# X=E
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
			elseif {[peek $p1] == 0x66} { ;# X=H
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
			elseif {[peek $p1] == 0x6e} { ;# X=L
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
			elseif {[peek $p1] == 0x2a} {
				set word [expr [peek $p2] | ([peek $p3] << 8)]
				markdata $word
			} else { ;# op (IX+index)
				set index [peek $p2]
				markdata [expr $ix + $index]
			}
		}
		fd { ;# operation with IY somewhere
			;# LD X,(IY + index)
			if {[peek $p1] == 0x7e} { ;# X=A
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
			elseif {[peek $p1] == 0x46} { ;# X=B
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
			elseif {[peek $p1] == 0x4e} { ;# X=C
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
			elseif {[peek $p1] == 0x56} { ;# X=D
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
			elseif {[peek $p1] == 0x5e} { ;# X=E
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
			elseif {[peek $p1] == 0x66} { ;# X=H
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
			elseif {[peek $p1] == 0x6e} { ;# X=L
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
			elseif {[peek $p1] == 0x2a} {
				set word [expr [peek $p2] | ([peek $p3] << 8)]
				markdata $word
			} else { ;# op (IX+index)
				set index [peek $p2]
				markdata [expr $iy + $index]
			}
		}
	}
}

namespace export codeanalyzer

} ;# namespace codeanalyzer

namespace import codeanalyzer::*
