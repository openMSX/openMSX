# Return the content of the MSX screen as a string (text-modes only)

set_help_text getScreen \
{Returns the content of the MSX screen as a string (only works for text-modes).
}

proc getScreen {} {
	# screen detection
	set mode [peek 0xfcaf]
	switch $mode {
		0 {
			set addr 0
			set width [expr ([debug read "VDP regs" 0] & 0x04) ? 80 : 40]
		}
		1 {
			set addr 6144
			set width 32
		}
		default {
			error "Screen Mode $mode Not Supported"
		}
	}

	# scrape screen and build string
	set screen ""
	for {set y 0} {$y < 23} {incr y} {
		set line ""
		for {set x 0} {$x < $width} {incr x} {
			append line [format %c [debug read VRAM $addr]]
			incr addr
		}
		append screen [string trim $line] "\n"
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

proc listing {} {
	set tokens1 [list \
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
	set tokens2 [list \
		"" "LEFT$" "RIGHT$" "MID$" "SGN" "INT" "ABS" "SQR" "RND" "SIN" \
		"LOG" "EXP" "COS" "TAN" "ATN" "FRE" "INP" "POS" "LEN" "STR$" \
		"VAL" "ASC" "CHR$" "PEEK" "VPEEK" "SPACE$" "OCT$" "HEX$" \
		"LPOS" "BIN$" "CINT" "CSNG" "CDBL" "FIX" "STICK" "STRIG" "PDL" \
		"PAD" "DSKF" "FPOS" "CVI" "CVS" "CVD" "EOF" "LOC" "LOF" "MKI$" \
		"MK$" "MKD$" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" \
		"" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" \
		"" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" \
		"" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" "" ]

	# Loop over all lines
	set listing ""
	for {set addr [peek16 0xf676]} {[peek $addr] != 0} {} {
		append listing [format "0x%x > " $addr]
		incr addr 2
		append listing "[peek16 $addr] "
		incr addr 2
		# Loop over tokens in one line
		while {true} {
			set token [peek $addr] ; incr addr
			if {0x80 < $token && $token < 0xff} {
				set t [lindex $tokens1 [expr $token - 0x80]]
			} elseif {$token == 0xff} {
				set t [lindex $tokens2 [expr [peek $addr] - 0x80]]
				incr addr
			} elseif {$token == 0x3a} {
				switch [peek $addr] {
					0xa1    { set t "ELSE" ; incr addr }
					0x8f    { set t "'"    ; incr addr }
					default { set t ":"                }
				}
			} elseif {$token == 0x0} {
				break
			} elseif {$token == 0x0B} {
				set t [format "&O%o" [peek16 $addr]]
				incr addr 2
			} elseif {$token == 0x0C} {
				set t [format "&H%x" [peek16 $addr]]
				incr addr 2
			} elseif {$token == 0x0D} {
				# line number (stored as address)
				set t [format "0x%x" [expr [peek16 $addr] + 1]]
				incr addr 2
			} elseif {$token == 0x0E} {
				set t [format "%d" [peek16 $addr]]
				incr addr 2
			} elseif {$token == 0x0F} {
				set t [format "%d" [peek $addr]]
				incr addr
			} elseif {$token == 0x1C} {
				set t [format "%d" [peek16 $addr]]
				incr addr 2
			} elseif {$token == 0x1D} {
				set t "(TODO: Single)"
				incr addr
			} elseif {$token == 0x1F} {
				set t "(TODO: Double)"
				incr addr
			} elseif {0x11 <= $token && $token <= 0x1a} {
				set t [expr $token - 0x11]
			} else {
				set t [format "%c" $token]
			}
			append listing $t
		}
		append listing "\n"
	}
	return $listing
}
