# Convenience procs to read or write Z80 registers.
# Usage is similar to the builtin TCL proc 'set'
#
#  usage:
#    reg <reg>            read from a Z80 register
#    reg <reg> <value>    write to a Z80 register
#    
#      <reg> must be one of
#         A   F   B   C   D   E   H   L
#         A2  F2  B2  C2  D2  E3  H2  L2
#         IXl IXh IYl IYh PCh PCl SPh SPl
#         I   R   IM  IFF
#         AF  BC  DE  HL
#         AF2 BC2 DE2 HL2
#         IX  IY  PC  SP
#
#  examples:
#    reg E           read register E
#    reg HL          read register HL
#    reg C 7         write 7 to register C
#    reg AF 0x1234   write 0x12 to register A and 0x34 to F

set __regB(A)    0 ; set __regB(F)    1 ; set __regB(B)    2 ; set __regB(C)    3
set __regB(D)    4 ; set __regB(E)    5 ; set __regB(H)    6 ; set __regB(L)    7
set __regB(A2)   8 ; set __regB(F2)   9 ; set __regB(B2)  10 ; set __regB(C2)  11
set __regB(D2)  12 ; set __regB(E2)  13 ; set __regB(H2)  14 ; set __regB(L2)  15
set __regB(IXh) 16 ; set __regB(IXl) 17 ; set __regB(IYh) 18 ; set __regB(IYl) 19
set __regB(PCh) 20 ; set __regB(PCl) 21 ; set __regB(SPh) 22 ; set __regB(SPl) 23
set __regB(I)   24 ; set __regB(R)   25 ; set __regB(IM)  26 ; set __regB(IFF) 27

set __regW(AF)   0 ; set __regW(BC)   2 ; set __regW(DE)   4 ; set __regW(HL)   6 
set __regW(AF2)  8 ; set __regW(BC2) 10 ; set __regW(DE2) 12 ; set __regW(HL2) 14
set __regW(IX)  16 ; set __regW(IY)  18 ; set __regW(PC)  20 ; set __regW(SP)  22

proc reg { name { val "" } } {
	if [info exists ::__regB($name)] {
		set i $::__regB($name)
		set single 1
	} elseif [info exists ::__regW($name)] {
		set i $::__regW($name)
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

proc __cw { reg } { format "%04X" [reg $reg] }
proc __cb { reg } { format "%02X" [reg $reg] }
proc cpuregs {} {
	puts "AF =[__cw AF]  BC =[__cw BC]  DE =[__cw DE]  HL =[__cw HL]"
	puts "AF'=[__cw AF2]  BC'=[__cw BC2]  DE'=[__cw DE2]  HL'=[__cw HL2]"
	puts "IX =[__cw IX]  IY =[__cw IY]  PC =[__cw PC]  SP =[__cw SP]"
	puts "I  =[__cb I]    R  =[__cb R]    IM =[__cb IM]    IFF=[__cb IFF]"
}
