# tcl_bridge v1.0 (c) 2025 Pedro de Medeiros
# There is an example program that consumes this script at:
# https://github.com/pvmm/openmsx-tcl-bridge
#
# To execute a Tcl command from the MSX side, you need to perform the following
# steps:
#
# 1) Make sure the 'tcl_bridge' is active.
#
# To do this execute the command 'tcl_bridge start' in the console.
# The `tcl_bridge' is *intentionally* not active by default.
#
# 2) The MSX program needs to setup a control block.
#
# That control block has 6 fields
#
#   cmd_addr:      +0 (output, 2 bytes): pointer to the command string (plain string, no terminator needed)
#   cmd_len:       +2 (output, 2 bytes): length of the command string
#   res_addr:      +4 (output, 2 bytes): pointer to the result buffer
#   res_max_len:   +6 (output, 2 bytes): length of the result buffer
#
#   res_real_len:  +8 (input,  2 bytes): actual length of the result
#   status:       +10 (input,  1 byte ): success (0) or failure (1)
#
# Only the first 4 fields need to be filled in by the MSX program, the last 2
# fields are filled in by the 'tcl_bridge' (see below).
#
# 3) Next the MSX program needs to send the address (2 bytes) of this control
# block to I/O port '6'. This will trigger the 'tcl_bridge'.
#
# 4) The 'tcl_bridge' then executes the given Tcl command (located at 'cmd_addr').
#
# If the command was executed successful, a '0' is written to the 'status' field
# in the control block'. If there was an error a '1' is written.
#
# The result string (successful result or the error message) is written into the
# result buffer located at 'res_addr'.
#
# The 'tcl_bridge' only writes at most 'res_max_len' bytes into that buffer. So
# if the result buffer is too small the result gets truncated. The result is
# written WITHOUT terminator characters. The full (non-truncated) length is
# written to the 'res_real_len' field of the control block.
#
# 5) The MSX program should then check the result.
#
# It should check success / failure by examining the 'status' field. Possibly
# check for overflow by comparing the actual length (field 'res_real_len') with
# the given buffer length (field 'res_max_len').
#
# If the MSX want to detect whether the 'tcl_bridge' is active, it can e.g.
# write 0xff to the 'status' field and then check if it was changed into '0'
# or '1'.
#
namespace eval tcl_bridge {

variable wp {}
variable latch 0
variable address

set_help_text tcl_bridge \
{tcl_bridge -- allows guest/host communication through a MSX-to-Tcl bridge.

Usage:
  tcl_bridge start      Create communication channel
  tcl_bridge stop       Destroy communication channel
}

proc tcl_bridge {cmd} {
    switch -- $cmd {
        start   { tcl_bridge::start }
        stop    { tcl_bridge::stop }
        default { error "Unknown sub-command \"$cmd\"." }
    }
}

proc start {} {
    variable wp
    if {$wp eq {}} {
        set wp [debug watchpoint create -type write_io -address 6 -command {tcl_bridge::read_command $::wp_last_value}]
    }
}

proc stop {} {
    variable wp
    if {$wp ne {}} {
        debug watchpoint remove $wp
        set wp {}
    }
}

proc min {x y} {
    expr {$x < $y ? $x : $y}
}

proc execute_command {base} {
    set cmd_addr     [peek16 $base]
    set cmd_len      [peek16 [expr {$base + 2}]]
    set res_addr     [peek16 [expr {$base + 4}]]
    set res_max_len  [peek16 [expr {$base + 6}]]
    set res_real_len [expr {$base + 8}]
    set status       [expr {$base + 10}]
    set cmd [debug read_block memory $cmd_addr $cmd_len]
    # execute command
    set response [catch {set output [eval $cmd]} output]
    # write command result and output data
    debug write_block memory $res_real_len [binary format s [min [string length $output] 0xFFFF]]
    debug write_block memory $res_addr [string range $output 0 [min [string length $output] $res_max_len]-1]]
    debug write memory $status $response
}

proc read_command {byte} {
    variable latch
    variable address
    switch -- $latch {
        0 {
            set address $byte
            set latch 1
        }
        1 {
            incr address [expr {$byte << 8}]
            set latch 0
            execute_command $address
        }
    }
}

namespace export tcl_bridge

} ;# namespace tcl_bridge

namespace import tcl_bridge::*
