# tcl_bridge v1.0 (c) 2025 Pedro de Medeiros
namespace eval tcl_bridge {

variable wp {}
variable latch 0
variable address
variable result

set_help_text tcl_bridge \
{tcl_bridge -- allows guest/host communication through a MSX-DOS-to-Tcl bridge.

Usage:
  tcl_bridge start      Create communication channel
  tcl_bridge stop       Destroy communication channel
}

proc tcl_bridge {args} {
    if {[catch {set result [tcl_bridge::dispatcher {*}$args]} msg]} {
        puts stderr $::errorInfo
        error $msg
    }
    return $result
}

proc dispatcher {args} {
    set params "[lrange $args 1 end]"
    set cmd [lindex $args 0]
    switch -- $cmd {
        start   { return [tcl_bridge::start {*}$params] }
        stop    { return [tcl_bridge::stop  {*}$params] }
        default { error "Unknown command \"[lindex $args 0]\"." }
    }
}

proc start {} {
    variable wp
    if {$wp eq {}} {
        puts "tcl_bridge v1.0 (c) 2025 Pedro de Medeiros."
        set wp [debug watchpoint create -type write_io -address 6 -command {tcl_bridge::read_command $::wp_last_value}]
        set init 1
        puts "tcl_bridge initialized"
    } else {
        puts "tcl_bridge already running"
    }
}

proc stop {} {
    variable wp
    if {$wp ne {}} {
        debug watchpoint remove $wp
        set wp {}
        puts "tcl_bridge stopped"
    } else {
        puts "tcl_bridge not running"
    }
}

proc read_data {base} {
    set iaddr [peek16 $base]            ;# input address
    set oaddr [peek16 [incr base 2]]    ;# output address
    set osize [peek16 [incr base 2]]    ;# output size
    set s ""
    for {set addr $iaddr} {[set c [peek $addr]] != 0} {incr addr} {
        append s [format %c $c]
        # detect overflows and runaways
        if {$addr < $iaddr || $addr > $iaddr + 256} { break }
    }
    if {$c == 0} {
        # execute command
        if {[catch {eval $s} msg]} {
            puts "Caught error: $msg"
        } else {
            puts OK
            set msg "OK"
        }
    } else {
        set msg "Parse error"
    }
    debug write_block memory $oaddr [concat [string range $msg 0 [expr {$osize - 2}]]$]
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
            read_data $address
        }
    }
}

namespace export tcl_bridge

} ;# namespace tcl_bridge

namespace import tcl_bridge::*
