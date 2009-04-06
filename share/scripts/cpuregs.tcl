# reg command

set_help_text reg \
{Convenience procs to read or write Z80 registers.
Usage is similar to the builtin TCL proc 'set'

 usage:
   reg <reg>            read from a Z80 register
   reg <reg> <value>    write to a Z80 register

     <reg> must be one of
        A   F   B   C   D   E   H   L
        A2  F2  B2  C2  D2  E3  H2  L2
        IXl IXh IYl IYh PCh PCl SPh SPl
        I   R   IM  IFF
        AF  BC  DE  HL
        AF2 BC2 DE2 HL2
        IX  IY  PC  SP

 examples:
   reg E           read register E
   reg HL          read register HL
   reg C 7         write 7 to register C
   reg AF 0x1234   write 0x12 to register A and 0x34 to F
}

namespace eval cpuregs {

variable regB
variable regW

set regB(A)    0 ; set regB(F)    1 ; set regB(B)    2 ; set regB(C)    3
set regB(D)    4 ; set regB(E)    5 ; set regB(H)    6 ; set regB(L)    7
set regB(A2)   8 ; set regB(F2)   9 ; set regB(B2)  10 ; set regB(C2)  11
set regB(D2)  12 ; set regB(E2)  13 ; set regB(H2)  14 ; set regB(L2)  15
set regB(IXh) 16 ; set regB(IXl) 17 ; set regB(IYh) 18 ; set regB(IYl) 19
set regB(PCh) 20 ; set regB(PCl) 21 ; set regB(SPh) 22 ; set regB(SPl) 23
set regB(I)   24 ; set regB(R)   25 ; set regB(IM)  26 ; set regB(IFF) 27

set regW(AF)   0 ; set regW(BC)   2 ; set regW(DE)   4 ; set regW(HL)   6
set regW(AF2)  8 ; set regW(BC2) 10 ; set regW(DE2) 12 ; set regW(HL2) 14
set regW(IX)  16 ; set regW(IY)  18 ; set regW(PC)  20 ; set regW(SP)  22

set_tabcompletion_proc reg [namespace code __tab_reg] false

proc __tab_reg { args } {
	variable regB
	variable regW

	set r1 [array names regB]
	set r2 [array names regW]
	join [list $r1 $r2]
}

proc reg { name { val "" } } {
	variable regB
	variable regW

	set name [string toupper $name]
	if [info exists regB($name)] {
		set i $regB($name)
		set single 1
	} elseif [info exists regW($name)] {
		set i $regW($name)
		set single 0
	} else {
		error "Unknown Z80 register: $name"
	}
	set d "CPU regs"
	if { $val == "" } {
		if { $single } {
			return [debug read $d $i]
		} else {
			return [expr 256 * [debug read $d $i] + [debug read $d [expr $i + 1]]]
		}
	} else {
		if { $single } {
			debug write $d $i $val
		} else {
			debug write $d $i [expr $val / 256]
			debug write $d [expr $i + 1] [expr $val & 255]
		}
	}
}

namespace export reg

} ;# namespace cpuregs

# cpuregs command

set_help_text cpuregs "Gives an overview of all Z80 registers."

namespace eval cpuregs {

proc cw { reg } { format "%04X" [reg $reg] }
proc cb { reg } { format "%02X" [reg $reg] }

proc cpuregs {} {
	puts "AF =[cw AF]  BC =[cw BC]  DE =[cw DE]  HL =[cw HL]"
	puts "AF'=[cw AF2]  BC'=[cw BC2]  DE'=[cw DE2]  HL'=[cw HL2]"
	puts "IX =[cw IX]  IY =[cw IY]  PC =[cw PC]  SP =[cw SP]"
	puts "I  =[cb I]    R  =[cb R]    IM =[cb IM]    IFF=[cb IFF]"
}

namespace export cpuregs

} ;# namespace cpureegs

namespace import cpuregs::*
