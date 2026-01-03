namespace eval trace {

# cpu_trace

set_help_text cpu_trace \
{cpu_trace <item>...

Enable CPU instruction/register tracing for the given items.
 <item> can be one of the following:
    "instr":  to trace instructions
    <reg>: an 8 or 16 bit register name like 'A', 'B' or 'HL', 'PC'
           the same list of registers as accepted by the 'reg' command

For example execute:
    cpu_trace instr A HL
to trace the instruction together with the A and HL register.

To disable tracing again execute:
    cpu_trace
so pass no arguments (no enabled items).

Note this can be expensive and/or can quickly consume a lot of memory. The Z80 (and certainly R800) can execute almost 1 million instructions per second. So it's recommended to only enable those items you need and limit the trace duration to less than a minute.
}
set_tabcompletion_proc cpu_trace [namespace code cpu_trace_tab] false

variable cond_id
variable cpu_trace_list [list]

proc get_allowed_items {} {
	variable allowed_items_cache
	if {![info exists allowed_items_cache]} {
		reg A ; # ensure _cpuregs.tcl is loaded
		set allowed_items_cache [lsort [concat [dict keys $cpuregs::regB] [dict keys $cpuregs::regW] "INSTR"]]
	}
	return $allowed_items_cache
}

proc cpu_trace_tab {args} {
	return [get_allowed_items]
}

proc cpu_trace {args} {
	set allowed_items [get_allowed_items]
	foreach i $args {
		if {[string toupper $i] ni $allowed_items} {
			error "Invalid value for cpu_trace: $i, must be one of $allowed_items"
		}
	}

	variable cpu_trace_list
	set cpu_trace_list $args

	variable cond_id
	catch { debug condition remove $cond_id }
	if {[llength $cpu_trace_list] != 0} {
		set cond_id [debug condition create -command [namespace code do_cpu_trace]]
	}

	foreach e $cpu_trace_list {
		if {$e eq "instr"} {
			set inst [regsub -all { +} [lindex [debug disasm [reg pc]] 0] { }]
			debug trace add $e $inst -type string -description "CPU instructions"
		} else {
			debug trace add $e [reg $e] -type int -format hex -description "CPU register $e"
		}
	}
}
namespace export cpu_trace

proc do_cpu_trace {} {
	variable cpu_trace_list
	foreach e $cpu_trace_list {
		if {$e eq "instr"} {
			set inst [regsub -all { +} [lindex [debug disasm [reg pc]] 0] { }]
			debug trace add $e $inst
		} else {
			debug trace add $e -merge [reg $e]
		}
	}
}

} ;# namespace eval trace

namespace import trace::*
