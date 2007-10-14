# Catapult Helper File (for use with openMSX Catapult)

proc getScreen {} {
#screen detection

if {[peek 0xfcaf]>1} {return [concat "Screen Mode " [peek 0xfcaf] " Not Supported"]}

if {[peek 0xfcaf]==0} 	{
			#screen 0
				if {[debug read VDP\ regs 0] & 0x04} {set chars 80} else {set chars 40}
				set base 0
			} else {
			#screen 1
				set base 6144
				set chars 32

			}

set line ""
set screen ""

#scrape screen and build string
	for {set y 0} {$y < 23} {incr y } {
		set row [expr {$y * $chars}]
		for {set x 0} {$x < $chars} {incr x } {
			append line  [format %c [debug read VRAM [expr {$base + $x + $row}]]]
		}
	append screen [string trim $line]
	append screen "\n"
	set line ""
	}

return  [string trim $screen]
}

#*****************************************
#* Basic Reader
#*****************************************

# Helper Functions
proc base {base} {
	set base [expr {[peek [expr $base+1]]*256+[peek $base]}]
	return $base
}

proc getEnd {} {
	for {set i [base 0xf676]} {$i < 0xffff} {incr i} {
		if {[base $i]=="0"} {return $i}
	}
}

proc getInit {addr} {
	return [concat [format %x [base $addr]] " :: " [base [expr $addr+2]]]
}

# Main Function
proc getBasic {} {
	set startBasic [base 0xf676]
	set endBasic [getEnd]

	puts $startBasic
	puts $endBasic

	set newLine 1

	if {$startBasic == $endBasic} {return "No Listing in Memory"}

	for {set addr [expr $startBasic]} {$addr < $endBasic} {incr addr} {
		
		if {$newLine==1} {
			puts [getInit $addr]
			incr addr 4
			set newLine 0
		}

		set addrVal [peek $addr]
	
		set com ""

		if {$addrVal>=0x20 && $addrVal<=0x5A} {set com [format %c $addrVal]}
		if {$addrVal>=0x61 && $addrVal<=0x7A} {set com [format %c $addrVal]}
		if {$addrVal==0x97} {set com "DEF"}
		if {$addrVal==0x8F} {set com "REM"}



		if {$addrVal==0xf1} {set com "+"}
		if {$addrVal==0xf2} {set com "-"}
		if {$addrVal==0xf3} {set com "*"}
		if {$addrVal==0xf4} {set com "/"}
		if {$addrVal==0xf5} {set com "^"}
		if {$addrVal==0xfc} {set com "\\"}

		if {$addrVal==0x20} {set com " "}
		if {$addrVal==0x22} {set com "\""}
		if {$addrVal==0x91} {set com "print"}

		if {$addrVal==0x11} {set com "1"}
		if {$addrVal==0x12} {set com "2"}
		if {$addrVal==0x13} {set com "3"}
		if {$addrVal==0x14} {set com "4"}
		if {$addrVal==0x15} {set com "5"}
		if {$addrVal==0x16} {set com "6"}
		if {$addrVal==0x17} {set com "7"}
		if {$addrVal==0x18} {set com "8"}
		if {$addrVal==0x19} {set com "9"}
		if {$addrVal==0x1a} {set com "0"}
		if {$addrVal==0xef} {set com "="}
		if {$addrVal==0x3a} {set com ":"}

		#BASIC COMMANDS
	if {$addrVal==0xFF || $addrVal==0x3A} {
		incr addr 1
		set addrVal [peek $addr]
		if {$addrVal==0xE0} {set com "NOT"}
		if {$addrVal==0xA8} {set com "DELETE"}
		if {$addrVal==0x30} {set com "MKD$"}
		if {$addrVal==0x2F} {set com "MKS$"}
		if {$addrVal==0xEB} {set com "OFF"}
		if {$addrVal==0xF7} {set com "OR"}
		if {$addrVal==0x97} {set com "DEF"}
		if {$addrVal==0x86} {set com "DIM"}
		if {$addrVal==0x6} {set com "ABS"}
		if {$addrVal==0xF6} {set com "AND"}
		if {$addrVal==0x15} {set com "ASC"}
		if {$addrVal==0x0E} {set com "ATN"}
		if {$addrVal==0xE9} {set com "ATTR$"}
		if {$addrVal==0xA9} {set com "AUTO"}
		if {$addrVal==0xC9} {set com "BASE"}
		if {$addrVal==0xC0} {set com "BEEP"}
		if {$addrVal==0x1D} {set com "BIN$"}
		if {$addrVal==0xCF} {set com "BLOAD"}
		if {$addrVal==0xD0} {set com "BSAVE"}
		if {$addrVal==0xCA} {set com "CALL"}
		if {$addrVal==0x20} {set com "CDBL"}
		if {$addrVal==0x16} {set com "CHR$"}
		if {$addrVal==0x1E} {set com "CINT"}
		if {$addrVal==0xBC} {set com "CIRCLE"}
		if {$addrVal==0x92} {set com "CLEAR"}
		if {$addrVal==0x9B} {set com "CLOAD"}
		if {$addrVal==0x9F} {set com "CLS"}
		if {$addrVal==0xD7} {set com "CMD"}
		if {$addrVal==0xBD} {set com "COLOR"}
		if {$addrVal==0x99} {set com "CONT"}
		if {$addrVal==0xD6} {set com "COPY"}
		if {$addrVal==0x0C} {set com "COS"}
		if {$addrVal==0xE8} {set com "CRSLIN"}
		if {$addrVal==0x9A} {set com "CSAVE"}
		if {$addrVal==0x1F} {set com "CSNG"}
		if {$addrVal==0x2A} {set com "CVD"}
		if {$addrVal==0x28} {set com "CVI"}
		if {$addrVal==0x29} {set com "CVS"}
		if {$addrVal==0x84} {set com "DATA"}
		if {$addrVal==0xAE} {set com "DEFDBL"}
		if {$addrVal==0xAC} {set com "DEFINT"}
		if {$addrVal==0xAD} {set com "DEFSNG"}
		if {$addrVal==0xAB} {set com "DEFSTR"}
		if {$addrVal==0xBE} {set com "DRAW"}
		if {$addrVal==0x26} {set com "DSKF"}
		if {$addrVal==0xEA} {set com "DSKI$"}
		if {$addrVal==0xD1} {set com "DSKO$"}
		if {$addrVal==0xA1} {set com "ELSE"}
		if {$addrVal==0x81} {set com "END"}
		if {$addrVal==0x2B} {set com "EOF"}
		if {$addrVal==0xF9} {set com "EQU"}
		if {$addrVal==0xA5} {set com "ERASE"}
		if {$addrVal==0xE1} {set com "ERL"}
		if {$addrVal==0xE2} {set com "ERR"}
		if {$addrVal==0xA6} {set com "ERROR"}
		if {$addrVal==0x0B} {set com "EXP"}
		if {$addrVal==0xB1} {set com "FIELD"}
		if {$addrVal==0xB7} {set com "FILES"}
		if {$addrVal==0x21} {set com "FIX"}
		if {$addrVal==0xDE} {set com "FN"}
		if {$addrVal==0x82} {set com "FOR"}
		if {$addrVal==0x27} {set com "FPOS"}
		if {$addrVal==0x0F} {set com "FRE"}
		if {$addrVal==0xB2} {set com "GET"}
		if {$addrVal==0x8D} {set com "GOSUB"}
		if {$addrVal==0x89} {set com "GOTO"}
		if {$addrVal==0x1B} {set com "HEX$"}
		if {$addrVal==0x8B} {set com "IF"}
		if {$addrVal==0xFA} {set com "IMP"}
		if {$addrVal==0xEC} {set com "INKEY$"}
		if {$addrVal==0x10} {set com "INP"}
		if {$addrVal==0x85} {set com "INPUT"}
		if {$addrVal==0xE5} {set com "INSTR"}
		if {$addrVal==0x5} {set com "INT"}
		if {$addrVal==0xD5} {set com "IPL"}
		if {$addrVal==0xCC} {set com "KEY"}
		if {$addrVal==0xD4} {set com "KILL"}
		if {$addrVal==0x2D} {set com "LDF"}
		if {$addrVal==0x1} {set com "LEFT$"}
		if {$addrVal==0x12} {set com "LEN"}
		if {$addrVal==0x88} {set com "LET"}
		if {$addrVal==0xBB} {set com "LFILES"}
		if {$addrVal==0xAF} {set com "LINE"}
		if {$addrVal==0x93} {set com "LIST"}
		if {$addrVal==0x9E} {set com "LLIST"}
		if {$addrVal==0xB5} {set com "LOAD"}
		if {$addrVal==0x2C} {set com "LOC"}
		if {$addrVal==0xD8} {set com "LOCATE"}
		if {$addrVal==0x0A} {set com "LOG"}
		if {$addrVal==0x1C} {set com "LPOS"}
		if {$addrVal==0x9D} {set com "LPRINT"}
		if {$addrVal==0xB8} {set com "LSET"}
		if {$addrVal==0xCD} {set com "MAX"}
		if {$addrVal==0xB6} {set com "MERGE"}
		if {$addrVal==0x3} {set com "MID$"}
		if {$addrVal==0x2E} {set com "MKI$"}
		if {$addrVal==0xFB} {set com "MOD"}
		if {$addrVal==0xCE} {set com "MOTOR"}
		if {$addrVal==0xD3} {set com "NAME"}
		if {$addrVal==0x94} {set com "NEW"}
		if {$addrVal==0x83} {set com "NEXT"}
		if {$addrVal==0x1A} {set com "OCT$"}
		if {$addrVal==0x95} {set com "ON"}
		if {$addrVal==0xB0} {set com "OPEN"}
		if {$addrVal==0x9C} {set com "OUT"}
		if {$addrVal==0x25} {set com "PAD"}
		if {$addrVal==0xBF} {set com "PAINT"}
		if {$addrVal==0x24} {set com "PDL"}
		if {$addrVal==0x17} {set com "PEEK"}
		if {$addrVal==0xC1} {set com "PLAY"}
		if {$addrVal==0xED} {set com "POINT"}
		if {$addrVal==0x98} {set com "POKE"}
		if {$addrVal==0x11} {set com "POS"}
		if {$addrVal==0xC3} {set com "PRESET"}
		if {$addrVal==0x91} {set com "PRINT"}
		if {$addrVal==0xC2} {set com "PSET"}
		if {$addrVal==0x87} {set com "READ"}
		if {$addrVal==0x8F} {set com "REM"}
		if {$addrVal==0xAA} {set com "RENUM"}
		if {$addrVal==0x8C} {set com "RESTORE"}
		if {$addrVal==0xA7} {set com "RESUME"}
		if {$addrVal==0x8E} {set com "RETURN"}
		if {$addrVal==0x2} {set com "RIGHT$"}
		if {$addrVal==0x8} {set com "RND"}
		if {$addrVal==0xB9} {set com "RSET"}
		if {$addrVal==0x8A} {set com "RUN"}
		if {$addrVal==0xBA} {set com "SAVE"}
		if {$addrVal==0xC5} {set com "SCREEN"}
		if {$addrVal==0xD2} {set com "SET"}
		if {$addrVal==0x4} {set com "SGN"}
		if {$addrVal==0x9} {set com "SIN"}
		if {$addrVal==0xC4} {set com "SOUND"}
		if {$addrVal==0x19} {set com "SPACE$"}
		if {$addrVal==0xDF} {set com "SPC"}
		if {$addrVal==0xC7} {set com "SPRITE"}
		if {$addrVal==0x7} {set com "SQR"}
		if {$addrVal==0xDC} {set com "STEP"}
		if {$addrVal==0x22} {set com "STICK"}
		if {$addrVal==0x90} {set com "STOP"}
		if {$addrVal==0x13} {set com "STR$"}
		if {$addrVal==0x23} {set com "STRIG"}
		if {$addrVal==0xE3} {set com "STRING$"}
		if {$addrVal==0xA4} {set com "SWAP"}
		if {$addrVal==0xDB} {set com "TAB"}
		if {$addrVal==0x0D} {set com "TAN"}
		if {$addrVal==0xDA} {set com "THEN"}
		if {$addrVal==0xCB} {set com "TIME"}
		if {$addrVal==0xD9} {set com "TO"}
		if {$addrVal==0xA3} {set com "TROFF"}
		if {$addrVal==0xA2} {set com "TRON"}
		if {$addrVal==0xE4} {set com "USING"}
		if {$addrVal==0xDD} {set com "USR"}
		if {$addrVal==0x14} {set com "VAL"}
		if {$addrVal==0xE7} {set com "VARPTR"}
		if {$addrVal==0xC8} {set com "VDP"}
		if {$addrVal==0x18} {set com "VPEEK"}
		if {$addrVal==0xC6} {set com "VPOKE"}
		if {$addrVal==0x96} {set com "WAIT"}
		if {$addrVal==0xA0} {set com "WIDTH"}
		if {$addrVal==0xF8} {set com "XOR"}
}

		puts [concat [format %x $addr] " - " [format %x $addrVal] " - " $com]
		if {$addrVal=="0"} {set newLine 1}
	}
}

