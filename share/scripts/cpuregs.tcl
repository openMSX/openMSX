proc getword { address } {
	set hi [debug read "CPU regs" $address]
	set lo [debug read "CPU regs" [expr $address + 1]]
	set word [expr 256 * $hi + $lo]
	return [format "%04X" $word]
}
proc getbyte { address } {
	set byte [debug read "CPU regs" $address]
	return [format "%02X" $byte]
}
proc cpuregs {} {
	puts "AF =[getword  0]  BC =[getword  2]  DE =[getword  4]  HL =[getword  6]"
	puts "AF'=[getword  8]  BC'=[getword 10]  DE'=[getword 12]  HL'=[getword 14]"
	puts "IX =[getword 16]  IY =[getword 18]  PC =[getword 20]  SP =[getword 22]"
	puts "I  =[getbyte 24]    R  =[getbyte 25]    IM =[getbyte 26]    IFF=[getbyte 27]"
}
