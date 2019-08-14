# omsxctl -- execute openMSX commands from within an MSX program
#
# This script should NOT be installed in the ~/.openmsx/share/scripts directory
# (because you don't want it to be active every time you start openMSX). Instead
# you should activate it via the openMSX command line with the option '-script
# omsxctl.tcl'.
#
# Typically used in combination with the MSX-DOS 'omsxctl.com' utility.
#
# But can also be used from any other MSX program with a code snippet like
# this:
#   ld   hl,command    ; pointer to the to be executed command
#   ld   de,result     ; the result of the command is written here
#   ld   bc,resultSize ; size of the result buffer (set to 0 if result is not needed)
#   out  (#2d),a       ; the value of A doesn't matter
#   jp   c,error       ; carry-flag set if there was an error executing the command
#                      ; BC is set to the actual length of the result string,
#                      ; or set to 0xffff when the result buffer was too small,
#                      ; in that case the result is still written to [DE] but truncated.
#
# Limitations:
#  Strings containing characters >=128 are not handled well:
#  - For passing the command string from the MSX to openMSX this could be
#    fixed (but is it worth it?).
#  - For passing the result from openMSX to the MSX I currently don't see a
#    solution that handles both arbitrary utf-8 strings and binary data (e.g.
#    created via the 'binary format' Tcl command). The current implementation
#    chooses to handle binary data (and of course pure ASCII strings work fine
#    as well).

proc trigger_omsxctl {} {
    # Read the command string from memory.
    set hl [reg HL]
    set cmd ""
    while 1 {
        set c [peek $hl]
        incr hl
        if {$c == 0} break
        append cmd [binary format c $c]
    }

    # Execute the command.
    set err [catch [list uplevel #0 $cmd] result]

    # Write the error status to the carry flag.
    reg F [expr {([reg F] & 0xfe) | $err}]

    # Write the result back to memory.
    set len [string length $result]
    set de [reg DE]
    set bc [reg BC]
    for {set i 0} {$i < $bc && $i < $len} {incr i} {
        binary scan [string index $result $i] c c
        poke [expr {$de + $i}] $c
    }

    # Write the actual result-length to BC.
    reg BC [expr {($len <= $bc) ? $len : 0xffff}]
}

# Register callback on write to IO-port 0x2d.
debug set_watchpoint write_io 0x2d 1 trigger_omsxctl
