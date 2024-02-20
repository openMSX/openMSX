namespace eval reverse {

# Enable reverse if not yet enabled. As an optimization, also return
# reverse-status info (so that caller doesn't have to query it again).
proc auto_enable {} {
	set stat_dict [reverse status]
	if {[dict get $stat_dict status] eq "disabled"} {
		reverse start
	}
	return $stat_dict
}

set_help_text reverse_prev \
{Go back in time to the previous 'snapshot'.
A 'snapshot' is actually an internal implementation detail, but the\
important thing for this command is that the further back in the past,\
the less dense the snapshots are. So executing this command multiple times\
will take successively bigger steps in the past. Going back to a snapshot\
is also slightly faster than going back to an arbitrary point in time\
(let's say going back a fixed amount of time).
}
proc reverse_prev {{minimum 1} {maximum 15}} {
	set stats [auto_enable]
	set snapshots [dict get $stats snapshots]
	set num_snapshots [llength $snapshots]
	if {$num_snapshots == 0} return

	set current [dict get $stats current]
	set upperTarget [expr {$current - $minimum}]
	set lowerTarget [expr {$current - $maximum}]

	# search latest snapshot that is still before upperTarget
	set i [expr {$num_snapshots - 1}]
	while {([lindex $snapshots $i] > $upperTarget) && ($i > 0)} {
		incr i -1
	}
	# but don't go below lowerTarget
	set t [lindex $snapshots $i]
	if {$t < $lowerTarget} {set t $lowerTarget}

	reverse goto $t
}

set_help_text reverse_next \
{This is very much like 'reverse_prev', but instead it goes to the closest\
snapshot in the future (if possible).
}
proc reverse_next {{minimum 0} {maximum 15}} {
	set stats [auto_enable]
	set snapshots [dict get $stats snapshots]
	set num_snapshots [llength $snapshots]
	if {$num_snapshots == 0} return

	set current [dict get $stats current]
	set lowerTarget [expr {$current + $minimum}]
	set upperTarget [expr {$current + $maximum}]

	# search first snapshot that is after lowerTarget
	lappend snapshots [dict get $stats end]
	set i 0
	while {($i < $num_snapshots) && ([lindex $snapshots $i] < $lowerTarget)} {
		incr i
	}

	if {$i < $num_snapshots} {
		# but don't go above upperTarget
		set t [lindex $snapshots $i]
		if {$t > $upperTarget} {set t $upperTarget}
		reverse goto $t
	}
}

proc goto_time_delta {delta} {
	set t [expr {[dict get [reverse status] current] + $delta}]
	if {$t < 0} {set t 0}
	reverse goto $t
}

proc go_back_one_step {} {
	goto_time_delta [expr {-$::speed / 100.0}]
}

proc go_forward_one_step {} {
	goto_time_delta [expr { $::speed / 100.0}]
}


# reverse bookmarks

variable bookmarks [dict create]

proc create_bookmark_from_current_time {name} {
	variable bookmarks
	dict set bookmarks $name [machine_info time]
	# The next message is useful as part of a hotkey command for this
#	message "Saved current time to bookmark '$name'"
	return "Created bookmark '$name' at [dict get $bookmarks $name]"
}

proc remove_bookmark {name} {
	variable bookmarks
	dict unset bookmarks $name
	return "Removed bookmark '$name'"
}

proc jump_to_bookmark {name} {
	variable bookmarks
	if {[dict exists $bookmarks $name]} {
		reverse goto [dict get $bookmarks $name]
		# The next message is useful as part of a hotkey command for
		# this
		#message "Jumped to bookmark '$name'"
	} else {
		error "Bookmark '$name' not defined..."
	}
}

proc clear_bookmarks {} {
	variable bookmarks
	set bookmarks [dict create]
}

proc save_bookmarks {name} {
	variable bookmarks

	set directory [file normalize $::env(OPENMSX_USER_DATA)/../reverse_bookmarks]
	file mkdir $directory
	set fullname [file join $directory ${name}.rbm]

	if {[catch {
		set the_file [open $fullname {WRONLY TRUNC CREAT}]
		puts $the_file $bookmarks
		close $the_file
	} errorText]} {
		error "Failed to save to $fullname: $errorText"
	}
	return "Successfully saved bookmarks to $fullname"
}

proc load_bookmarks {name} {
	variable bookmarks

	set directory [file normalize $::env(OPENMSX_USER_DATA)/../reverse_bookmarks]
	set fullname [file join $directory ${name}.rbm]

	if {[catch {
		set the_file [open $fullname {RDONLY}]
		set bookmarks [read $the_file]
		close $the_file
	} errorText]} {
		error "Failed to load from $fullname: $errorText"
	}

	return "Successfully loaded $fullname"
}

proc list_bookmarks_files {} {
	set directory [file normalize $::env(OPENMSX_USER_DATA)/../reverse_bookmarks]
	set results [list]
	foreach f [lsort [glob -tails -directory $directory -type f -nocomplain *.rbm]] {
		lappend results [file rootname $f]
	}
	return $results
}

proc reverse_bookmarks {subcmd args} {
	switch -- $subcmd {
		"create" {create_bookmark_from_current_time {*}$args}
		"remove" {remove_bookmark  {*}$args}
		"goto"   {jump_to_bookmark {*}$args}
		"clear"  {clear_bookmarks}
		"load"   {load_bookmarks   {*}$args}
		"save"   {save_bookmarks   {*}$args}
		default  {error "Invalid subcommand: $subcmd"}
	}
}

set_help_proc reverse_bookmarks [namespace code reverse_bookmarks_help]

proc reverse_bookmarks_help {args} {
	switch -- [lindex $args 1] {
		"create"    {return {Create a bookmark at the current time with the given name.

Syntax: reverse_bookmarks create <name>
}}
		"remove" {return {Remove the bookmark with the given name.

Syntax: reverse_bookmarks remove <name>
}}
		"goto"   {return {Go to the bookmark with the given name.

Syntax: reverse_bookmarks goto <name>
}}
		"clear"  {return {Removes all bookmarks.

Syntax: reverse_bookmarks clear
}}
		"save"   {return {Save the current reverse bookmarks to a file.

Syntax: reverse_bookmarks save <filename>
}}
		"load"   {return {Load reverse bookmarks from file.

Syntax: reverse_bookmarks load <filename>
}}
		default {return {Control the reverse bookmarks functionality.

Syntax:  reverse_bookmarks <sub-command> [<arguments>]
Where sub-command is one of:
    create   Create a bookmark at the current time
    remove   Remove a previously created bookmark
    goto     Go to a previously created bookmark
    clear    Shortcut to remove all bookmarks
    save     Save current bookmarks to a file
    load     Load previously saved bookmarks

Use 'help reverse_bookmarks <sub-command>' to get more detailed help on a specific sub-command.
}}
	}
}

set_tabcompletion_proc reverse_bookmarks [namespace code reverse_bookmarks_tabcompletion]
proc reverse_bookmarks_tabcompletion {args} {
	variable bookmarks

	if {[llength $args] == 2} {
		return [list "create" "remove" "goto" "clear" "save" "load"]
	} elseif {[llength $args] == 3} {
		switch -- [lindex $args 1] {
			"remove" -
			"goto"  {return [dict keys $bookmarks]}
			"load"  -
			"save"  {return [list_bookmarks_files]}
			default {return [list]}
		}
	}
}

### auto save replays

trace add variable ::auto_save_replay "write" [namespace code auto_save_setting_changed]

variable old_auto_save_value $::auto_save_replay
variable auto_save_after_id 0

proc auto_save_setting_changed {name1 name2 op} {
	variable old_auto_save_value
	variable auto_save_after_id

	if {$::auto_save_replay != $old_auto_save_value} {
		set old_auto_save_value $::auto_save_replay

		if {$::auto_save_replay && $auto_save_after_id == 0 } {
			set stat_dict [reverse status]
			if {[dict get $stat_dict status] eq "disabled"} {
				error "Reverse is not enabled!"
			}
			auto_save_replay_loop
			puts "Enabled auto-save of replay to filename $::auto_save_replay_filename every $::auto_save_replay_interval seconds."
		} elseif {!$::auto_save_replay && $auto_save_after_id != 0 } {
			after cancel $auto_save_after_id
			set auto_save_after_id 0
			puts "Auto-save of replay disabled."
		}
	}
}

proc auto_save_replay_loop {} {
	variable auto_save_after_id

	if {$::auto_save_replay} {
		reverse savereplay -maxnofextrasnapshots 0 $::auto_save_replay_filename

		set auto_save_after_id [after realtime $::auto_save_replay_interval "reverse::auto_save_replay_loop"]
	}
}

namespace export reverse_prev
namespace export reverse_next
namespace export goto_time_delta
namespace export go_back_one_step
namespace export go_forward_one_step
namespace export reverse_bookmarks

} ;# namespace reverse

namespace import reverse::*
