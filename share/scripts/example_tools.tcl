# Return the content of the MSX screen as a string (text-modes only)

set_help_text getScreen \
{Returns the content of the MSX screen as a string (only works for text-modes).
}

proc getScreen {} {
	# screen detection
	if {[peek 0xfcaf] > 1} {
		return [concat "Screen Mode " [peek 0xfcaf] " Not Supported"]
	}
	if {[peek 0xfcaf] == 0} {
		# screen 0
		if {[debug read VDP\ regs 0] & 0x04} {
			set chars 80
		} else {
			set chars 40
		}
		set word 0
	} else {
		# screen 1
		set word 6144
		set chars 32
	}

	set line ""
	set screen ""

	# scrape screen and build string
	for {set y 0} {$y < 23} {incr y } {
		set row [expr {$y * $chars}]
		for {set x 0} {$x < $chars} {incr x } {
			append line [format %c [debug read VRAM [expr {$word + $x + $row}]]]
		}
		append screen [string trim $line]
		append screen "\n"
		set line ""
	}

	return [string trim $screen]
}

#***********************************************
#* Basic Reader (version 0.1)
#*
#* A special thanks to Vincent van Dam for
#* giving me permission to translate his
#* script into TCL
#***********************************************

set_help_text listing \
{Interpret the content of the memory as a BASIC program and return the equivalent output of the BASIC LIST command. (May not be terribly useful, but it does show the power of openMSX scripts ;-)
}

proc __getLineNumber {addr} {
	#return "\n[peek16 [expr $addr + 2]] "
	return "\n0x[format %x [peek16 $addr]] > [peek16 [expr $addr + 2]] "
}

proc listing {} {
	set token1 [list \
		"" "END" "FOR" "NEXT" "DATA" "INPUT" "DIM" "READ" "LET" \
		"GOTO" "RUN" "IF" "RESTORE" "GOSUB" "RETURN" "REM" "STOP" \
		"PRINT" "CLEAR" "LIST" "NEW" "ON" "WAIT" "DEF" "POKE" "CONT" \
		"CSAVE" "CLOAD" "OUT" "LPRINT" "LLIST" "CLS" "WIDTH" "ELSE" \
		"TRON" "TROFF" "SWAP" "ERASE" "ERROR" "RESUME" "DELETE" \
		"AUTO" "RENUM" "DEFSTR" "DEFINT" "DEFSNG" "DEFDBL" "LINE" \
		"OPEN" "FIELD" "GET" "PUT" "CLOSE" "LOAD" "MERGE" "FILES" \
		"LSET" "RSET" "SAVE" "LFILES" "CIRCLE" "COLOR" "DRAW" "PAINT" \
		"BEEP" "PLAY" "PSET" "PRESET" "SOUND" "SCREEN" "VPOKE" \
		"SPRITE" "VDP" "word" "CALL" "TIME" "KEY" "MAX" "MOTOR" \
		"BLOAD" "BSAVE" "DSKO$" "SET" "NAME" "KILL" "IPL" "COPY" "CMD" \
		"LOCATE" "TO" "THEN" "TABC" "STEP" "USR" "FN" "SPCL" "NOT" \
		"ERL" "ERR" "STRING$" "USING" "INSRT" "" "VARPTR" "CSRLIN" \
		"ATTR$" "DSKI$" "OFF" "INKEY$" "POINT" ">" "=" "<" "+" "-" "*" \
		"/" "^" "AND" "OR" "XOR" "EQV" "IMP" "MOD" "\\" "" "" \
		"{escape-code}" ]
	set token2 [list \
		"" "LEFT$" "RIGHT$" "MID$" "SGN" "INT" "ABS" "SQR" "RND" "SIN" \
		"LOG" "EXP" "COS" "TAN" "ATN" "FRE" "INP" "POS" "LEN" "STR$" \
		"VAL" "ASC" "CHR$" "PEEK" "VPEEK" "SPACE$" "OCT$" "HEX$" \
		"LPOS" "BIN$" "CINT" "CSNG" "CDBL" "FIX" "STICK" "STRIG" "PDL" \
		"PAD" "DSKF" "FPOS" "CVI" "CVS" "CVD" "EOF" "LOC" "LOF" "MKI$" \
		"MK$" "MKD$" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" \
		"" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" \
		"" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" \
		"" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" ]

	set go 1
	set addr [peek16 0xf676]
	set listing ""
	set tok ""

	# Check for listing
	if {[peek16 $addr] == 0} {
		return "No Listing In Memory"
	}

	# Initial Line Number
	append listing [__getLineNumber $addr]
	incr addr 4

	# Loop through the memory
	while {$go == 1} {
		set tok ""
		if {[peek $addr] > 0x80 && [peek $addr] < 0xff} {
			set tok [lindex $token1 [expr [peek $addr] - 0x80]]
		} else {
			if {[peek $addr] == 0xff} {
				incr addr
				set tok [lindex $token2 [expr [peek $addr]-0x80]]
			} else {
				if {[peek $addr] == 0x3a} {
					set forward 0
					incr addr
					if {[peek $addr] == 0xa1} {
						set tok "ELSE"
						set forward 1
					}
					if {[peek $addr] == 0x8f} {
						set tok "'"
						set forward 1
					}
					if {$forward == 0} {
						set tok ":"
						incr addr -1
					}
				} else {
					set forward 0
					if {[peek $addr] == 0x0} {
						incr addr
						set tok [__getLineNumber $addr]
						incr addr 3
						set forward 1
					}
					if {[peek $addr] == 0x0B && $forward == 0} {
						incr addr
						set tok [format "&O%o" [peek16 $addr]]
						incr addr
						set forward 1
					}
					if {[peek $addr] == 0x0C && $forward == 0} {
						incr addr
						set tok [format "&H%x" [peek16 $addr]]
						incr addr
						set forward 1
					}
					if {[peek $addr] == 0x0D && $forward == 0} {
						set tok "(TODO GOSUB)"
						incr addr 2
						set forward 1
					}
					if {[peek $addr] == 0x0E && $forward == 0} {
						incr addr
						set tok [format "%d" [peek16 $addr]]
						incr addr
						set forward 1
					}
					if {[peek $addr] == 0x0F && $forward == 0} {
						incr addr
						set tok [format "%d" [peek $addr]]
						set forward 1
					}
					if {[peek $addr] == 0x1C && $forward == 0} {
						incr addr
						set tok [format "%d" [peek16 $addr]]
						incr addr
						set forward 1
					}
					if {[peek $addr] == 0x1D && $forward == 0} {
						incr addr
						set tok "(TODO: Single)"
						set forward 1
					}
					if {[peek $addr] == 0x1F && $forward == 0} {
						incr addr
						set tok "(TODO: Double)"
						set forward 1
					}
					if {[peek $addr] >= 0x11 && [peek $addr] <= 0x1a} {
						set tok [expr [peek $addr] - 0x11]
						set forward 1
					}
					if {$forward == 0} {
						set tok [format "%c" [peek $addr]]
					}
				}
			}
		}

		incr addr
		append listing $tok
		if {[peek16 $addr] == 0 || $addr >= 0xffff} {
			set go 0
		}
	}
	return $listing
}
