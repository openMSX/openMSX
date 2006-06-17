# 
# Binary format PSG logger
#
# (see http://www.msx.org/forumtopic6258.html for more context)
#
# Shiru wrote:
# 
#  Is it possible to make output in standart binary *.psg format (used in
#  emulators like x128, very-very old version of fMSX, Z80Stealth and some other)?
#
#  Header:
#
#   +0 4 Signature #50 #53 #47 #1A ('PSG' and byte #1A)
#   +4 1 Version number
#   +5 1 Interrupts freq. (50/60)
#   +6 10 Unused
#
#  Note: only signature is necessary, many emulators just fill other bytes by zero.
# 
#  Data stream:
# 
#  #00..#FC - register number, followed by data byte (value for that register)
#  #FD - EOF (usually not used in real logs, and not all progs can handle it)
#  #FE - number of interrupts (followed byte with number of interrupts/4, usually not used)
#  #FF - single interrupt
#

set_help_text psg_log \
{This script logs PSG registers 0 through 14 to a file at every frame end.
The file format is the PSG format used in some emulators. More information
is here:  http://www.msx.org/forumtopic6258.html (and in the comments of
this script).

Usage:
   psg_log start <filename>  start logging PSG registers to <filename> 
                             (default: log.psg)
   psg_log stop              stop logging PSG registers

Examples:
   psg_log start             start logging registers to default file log.psg
   psg_log start myfile.psg  start logging to file myfile.psg
   psg_log stop              stop logging PSG registers
}

set_tabcompletion_proc psg_log tab_psg_log
proc tab_psg_log { args } {
	if {[llength $args] == 2} {
		return "start stop"
	}
}

set __psg_log_file -1

proc psg_log { subcommand {filename "log.psg"} } {
	global __psg_log_file
	if [string equal $subcommand "start"] {
		set __psg_log_file [open $filename {WRONLY TRUNC CREAT}]
		fconfigure $__psg_log_file -translation binary
		set header "0x50 0x53 0x47 0x1A 0 0 0 0 0 0 0 0 0 0 0 0"
		puts -nonewline $__psg_log_file [binary format c16 $header]
		__do_psg_log
		return ""
	} elseif [string equal $subcommand "stop"] {
		close $__psg_log_file
		set __psg_log_file -1
		return ""
	} else {
		error "bad option \"$subcommand\": must be start, stop"
	}
}

proc __do_psg_log {} {
	global __psg_log_file
	if {$__psg_log_file == -1} return
	for {set i 0} {$i < 14} {incr i} {
		set value [debug read "PSG regs" $i]
		puts -nonewline $__psg_log_file [binary format c2 "$i $value"]
	}
	puts -nonewline $__psg_log_file [binary format c 0xFF]
	after frame __do_psg_log
}
