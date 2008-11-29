set_help_text trainer "
Usage:
  trainer                      see which trainer is currently active
  trainer <game>               see which cheats are currently active in the trainer
  trainer <game> all           activate all cheats in the trainer
  trainer <game> \[<cheat> ..\]  toggle cheats on/off
  trainer deactivate           deactivate trainers

Examples:
  trainer Frogger all
  trainer Circus\ Charlie 1 2
  trainer Pippols lives \"jump shoes\"\

When switching trainers, the currently active trainer will be deactivated.
"
set_tabcompletion_proc trainer __tab_trainer false
proc __tab_trainer {args} {
	if {[llength $args] == 2} {
		set result [array names ::__trainers]
		lappend result "deactivate"
	} else {
		set result [list]
		set name [lindex $args 1]
		if [info exists ::__trainers($name)] {
			set stuff $::__trainers($name)
			set items [lindex $stuff 0]
			set i 1
			foreach {item_name item_impl} $items {
				lappend result $item_name
				lappend result $i
				incr i
			}
		}
		lappend result "all"
	}
	return $result
}
proc trainer {args} {
	if {[llength $args] > 0} {
		set name [lindex $args 0]
		if {$name != "deactivate"} {
			set requested_items [lrange $args 1 end]
			if ![info exists ::__trainers($name)] {
				error "no trainer for $name."
			}
			set same_trainer [string equal $name $::__active_trainer]
			set items [__trainer_parse_items $name $requested_items]
			if $same_trainer {
				set new_items [list]
				foreach item1 $items item2 $::__trainer_items_active {
					lappend new_items [expr $item1 ^ $item2]
				}
				set ::__trainer_items_active $new_items
			} else {
				__trainer_deactivate
				set ::__active_trainer $name
				set ::__trainer_items_active $items
				__trainer_exec
			}
		} else {
			__trainer_deactivate
			return ""
		}
	}
	__trainer_print
}
proc __trainer_parse_items {name requested_items} {
	set stuff $::__trainers($name)
	set items [lindex $stuff 0]
	set result [list]
	set i 1
	foreach {item_name item_impl} $items {
		set active 0
		if {($requested_items == "all") ||
		  ([lsearch $requested_items $i] != -1) ||
		  ([lsearch $requested_items $item_name] != -1)} {
			set active 1
		}
		lappend result $active
		incr i
	}
	return $result
}
proc __trainer_print {} {
	if {$::__active_trainer == ""} {
		return "no trainer active"
	}
	set result [list]
	set stuff $::__trainers($::__active_trainer)
	set items [lindex $stuff 0]
	lappend result "active trainer: $::__active_trainer"
	set i 1
	foreach {item_name item_impl} $items item_active $::__trainer_items_active {
		set line "$i \["
		if $item_active {
			append line "x"
		} else {
			append line " "
		}
		append line "\] $item_name"
		lappend result $line
		incr i
	}
	join $result \n
}
proc __trainer_exec {} {
	set stuff $::__trainers($::__active_trainer)
	set items [lindex $stuff 0]
	set repeat [lindex $stuff 1]
	foreach {item_name item_impl} $items item_active $::__trainer_items_active {
		if $item_active {
			eval $item_impl
		}
	}
	set ::__trainer_after_id [eval "after $repeat __trainer_exec"]
}
proc __trainer_deactivate {} {
	if ![info exists ::__trainer_after_id] return ;# no trainer active
	after cancel $::__trainer_after_id
	unset ::__trainer_after_id
	set ::__active_trainer ""
}
proc __trainer_deactivate_after_boot {} {
	__trainer_deactivate
	after boot __trainer_deactivate_after_boot
}
__trainer_deactivate_after_boot

proc create_trainer {name repeat items} {
	set ::__trainers($name) [list $items $repeat]
}
proc poke {addr val} {debug write memory $addr $val}
proc peek {addr} {return [debug read memory $addr]}

# source the trainer definitions (user may override system defaults) and ignore errors
catch {source $env(OPENMSX_SYSTEM_DATA)/scripts/trainerdefs.tclinclude}
catch {source $env(OPENMSX_USER_DATA)/scripts/trainerdefs.tclinclude}

set ::__active_trainer ""
