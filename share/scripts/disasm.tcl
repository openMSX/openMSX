namespace eval disasm {

# very common debug functions

set_help_text peek \
{Read a byte from the given memory location.
Equivalent to "debug read memory <addr>".

usage:
  peek <addr>
}
proc peek {addr} {
	return [debug read memory $addr]
}

set_help_text peek16 \
{Read a 16-bit value (in little endian format) from the given memory location.

usage:
  peek16 <addr>
}
proc peek16 {addr} {
	return [expr [peek $addr] + 256 * [peek [expr $addr + 1]]]
}

set_help_text poke \
{Write a byte to the given memory location.
Equivalent to "debug write memory <addr> <val>".

usage:
  poke <addr> <val>
}
proc poke {addr val} {
	debug write memory $addr $val
}

set_help_text poke16 \
{Write a 16-bit value (in little endian format) to the given memory location.

usage:
  poke16 <addr> <val>
}
proc poke16 {addr val} {
	debug write memory       $addr      [expr $val & 255]
	debug write memory [expr $addr + 1] [expr $val >> 8]
}


#
# disasm
#
set_help_text disasm \
{Disassemble z80 instructions

Usage:
  disasm                Disassemble 8 instr starting at the currect PC
  disasm <addr>         Disassemble 8 instr starting at address <adr>
  disasm <addr> <num>   Disassemble <num> instr starting at address <addr>
}
proc disasm {{address -1} {num 8}} {
	if {$address == -1} { set address [reg PC] }
	for {set i 0} {$i < $num} {incr i} {
		set l [debug disasm $address]
		append result [format "%04X  %s\n" $address [join $l]]
		set address [expr $address + [llength $l] - 1]
	}
	return $result
}


#
# run_to
#
set_help_text run_to \
{Run to the specified address, if a breakpoint is reached earlier we stop
at that breakpoint.}
proc run_to {address} {
	set bp [debug set_bp $address]
	after break "debug remove_bp $bp"
	debug cont
}


#
# step_in
#
set_help_text step_in \
{Step in. Execute the next instruction, also go into subroutines.}
proc step_in {} {
	debug step
}


#
# step_out
#
set_help_text step_out \
{Step out of the current subroutine. In other words, execute till right after
the next 'ret' instruction (more if there were also extra 'call' instructions).
Note: simulation can be slow during execution of 'step_out', though for not
extremely large subroutines this is not a problem.}

variable step_out_bp1
variable step_out_bp2

proc step_out_is_ret {} {
	# ret        0xC9
	# ret <cc>   0xC0,0xC8,0xD0,..,0xF8
	# reti retn  0xED + 0x45,0x4D,0x55,..,0x7D
	set instr [peek16 [reg pc]]
	expr {(($instr & 0x00FF) == 0x00C9) ||
	      (($instr & 0x00C7) == 0x00C0) ||
	      (($instr & 0xC7FF) == 0x45ED)}
}
proc step_out_after_break {} {
	variable step_out_bp1
	variable step_out_bp2

	# also clean up when breaked, but not because of step_out
	catch { debug remove_condition $step_out_bp1 }
	catch { debug remove_condition $step_out_bp2 }
}
proc step_out_after_next {} {
	variable step_out_bp1
	variable step_out_bp2
	variable step_out_sp

	catch { debug remove_condition $step_out_bp2 }
	if {[reg sp] > $step_out_sp} {
		catch { debug remove_condition $step_out_bp1 }
		debug break
	}
}
proc step_out_after_ret {} {
	variable step_out_bp2

	catch { debug remove_condition $step_out_bp2 }
	set step_out_bp2 [debug set_condition 1 [namespace code step_out_after_next]]
}
proc step_out {} {
	variable step_out_bp1
	variable step_out_bp2
	variable step_out_sp

	catch { debug remove_condition $step_out_bp1 }
	catch { debug remove_condition $step_out_bp2 }
	set step_out_sp [reg sp]
	set step_out_bp1 [debug set_condition {[disasm::step_out_is_ret]} [namespace code step_out_after_ret]]
	after break [namespace code step_out_after_break]
	debug cont
}


#
# step_over
#
set_help_text step_over \
{Step over. Execute the next instruction but don't step into subroutines.
Only 'call' or 'rst' instructions are stepped over. Note: 'push xx / jp nn'
sequences can in theory also be used as calls but these are not skipped
by this command.}
proc step_over {} {
	set address [reg PC]
	set l [debug disasm $address]
	set instr [lindex $l 0]
	if {[string match "call*" $instr] ||
	    [string match "rst*"  $instr] ||
	    [string match "ldir*" $instr] ||
	    [string match "cpir*" $instr] ||
	    [string match "inir*" $instr] ||
	    [string match "otir*" $instr] ||
	    [string match "lddr*" $instr] ||
	    [string match "cpdr*" $instr] ||
	    [string match "indr*" $instr] ||
	    [string match "otdr*" $instr] ||
	    [string match "halt*" $instr]} {
		run_to [expr $address + [llength $l] - 1]
	} else {
		debug step
	}
}

#
# skip one instruction
#
set_help_text skip_instruction \
{Skip the current instruction. In other words increase the program counter with the length of the current instruction.}
proc skip_instruction {} {
	set pc [reg pc]
	reg pc [expr $pc + [llength [debug disasm $pc]] - 1]
}

namespace export peek
namespace export peek16
namespace export poke
namespace export poke16
namespace export disasm
namespace export run_to
namespace export step_over
namespace export step_out
namespace export step_in
namespace export skip_instruction

} ;# namespace disasm

namespace import disasm::*
