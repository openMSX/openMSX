###########################################################
#  openMSX TCL cheat finder version 0.4
#
# (c) copyright 2005 Patrick van Arkel all rights reserved
#
# Credits 
#
# - Maarten ter Huurne  for giving me hints and tips
#
# - Wouter for helping me simplify the script
# 
# - Johan van Leur for writing the bluemsx trainer
#   which inspired me to make this
#
#
# - findcheat_init start a new cheat search (mandantory for comparision) 
#
# Currently supported
# - findval [value] search with value
#
# - Comparision search 
#   Init with findcheat_init
#   * greater
#   * smaller
#   * equal
#   * notequal
#
# to use the cheat use this in the console
# debug write memory [address] [value]
#
# Example : 
# debug write memory 0x0256 0x99
# debug write memory 21056 99
#
# or a combination of the above
#
###########################################################

# set an array outside the procedures
# this will be used inside the procedures
set  memcomp(0) 666

# to init the whole process make a snapshot and look for a value
# if a value is not found at that address the value will be set to 999
# the number of 999 has been chosen because an 8-bit value can be 255 max

#  init (snapshot)
proc findcheat_init {} {
	global memcomp
	for { set i 0 } { $i <= 65535 } { incr i } {
		set memcomp($i) [debug read memory $i]
	}
}

# the 2nd step of the search fucntion. This will use the inital snapshot
# to look for values. If a value isn't found the value is again set to 999
# repeat this function until a value is found.

proc findval { cheatval } {
	global memcomp
	#check if previous search has been done
	if {$memcomp(0) == 666} { findcheat_init }

	for { set i 0 } { $i <= 65535 } { incr i } {
		if {$memcomp($i) != 999 } {
			if {[debug read memory $i] == $cheatval} {
				puts "addr [format 0x%04X $i] : has value $cheatval"
			} else {
				set memcomp($i) 999
			}
		}
	}
}

# ---------------------------------------------------
# Compare search bigger / smaller / equal / notequal
# ---------------------------------------------------

# search 

proc equal		{ a b } { expr "$a == $b" }
proc notequal		{ a b } { expr "$a != $b" }
proc smaller		{ a b } { expr "$a < $b"  }
proc smaller_or_equal	{ a b } { expr "$a <= $b" }
proc greater_or_equal	{ a b } { expr "$a >= $b" }
proc bigger		{ a b } { expr "$a > $b"  }

proc find { compare } {
	global memcomp
	for { set i 0 } { $i <= 65535 } { incr i } {
		if {$memcomp($i) != 999} {
			if {[$compare [debug read memory $i] $memcomp($i)]} {
				puts "addr [format 0x%04X $i] : has value [debug read memory $i] was $memcomp($i)"
				set memcomp($i) [debug read memory $i]
			} else {
				set memcomp($i) 999
			}
		}
	}
}
