# convenience wrappers around the low level savestate commands

namespace eval savestate {

proc savestate_common {} {
	uplevel {
		if {$name eq ""} {set name "quicksave"}
		set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
		set fullname [file join $directory ${name}.oms]
		set png [file join $directory ${name}.png]
	}
}

proc savestate {{name ""}} {
	savestate_common
	file mkdir $directory
	if {[catch {::openmsx::internal_screenshot -raw -doublesize $png}]} {
		# Creating the new screenshot failed, (try to) remove old screenshot to avoid confusion
		catch {file delete -- $png}
	}
	set currentID [machine]
	store_machine $currentID $fullname
	return $fullname
}

proc loadstate {{name ""}} {
	savestate_common
	set newID [restore_machine $fullname]
	set currentID [machine]
	if {$currentID ne ""} {delete_machine $currentID}
	activate_machine $newID
	return $name
}

# helper proc to get the raw savestate info
proc list_savestates_raw {} {
	set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
	set results [list]
	foreach f [glob -tails -directory $directory -nocomplain *.oms] {
		if       {[string range $f end-3 end] eq ".oms"} {
			set name [string range $f 0 end-4]
		} else {
			set name $f
		}
		set fullname [file join $directory $f]
		set filetime [file mtime $fullname]
		lappend results [list $name $filetime]
	}
	return $results
}

proc list_savestates {args} {
	set sort_key 0
	set sort_option "-ascii"
	set sort_order "-increasing"

	#parse options
	while (1) {
		switch -- [lindex $args 0] {
		"" break
		"-t" {
			set sort_key 1
			set sort_option "-integer"
			set args [lrange $args 1 end]
			set sort_order "-decreasing"
		}
		"default" {
			error "Invalid option: [lindex $args 0]"
		}
		}
	}

	set sorted_sublists [lsort ${sort_option} ${sort_order} -index $sort_key [list_savestates_raw]]

	set sorted_result [list]
	foreach sublist $sorted_sublists {lappend sorted_result [lindex $sublist 0]}
	return $sorted_result
}

proc delete_savestate {{name ""}} {
	savestate_common
	catch {file delete -- $fullname}
	catch {file delete -- $png}
	return ""
}

proc savestate_tab {args} {
	list_savestates
}

proc savestate_list_tab {args} {
	list "-t"
}

# savestate
set_help_text savestate \
{savestate [<name>]

Create a snapshot of the current emulated MSX machine.

Optionally you can specify a name for the savestate. If you omit this the default name 'quicksave' will be taken.

See also 'loadstate', 'list_savestates', 'delete_savestate'.
}
set_tabcompletion_proc savestate [namespace code savestate_tab]

# loadstate
set_help_text loadstate \
{loadstate [<name>]

Restore a previously created savestate.

You can specify the name of the savestate that should be loaded. If you omit this name, the default savestate will be loaded.

See also 'savestate', 'list_savestates', 'delete_savestate'.
}
set_tabcompletion_proc loadstate [namespace code savestate_tab]

# list_savestates
set_help_text list_savestates \
{list_savestates [options]

Return the names of all previously created savestates.

Options:
  -t      sort savestates by time

See also 'savestate', 'loadstate', 'delete_savestate'.
}
set_tabcompletion_proc list_savestates [namespace code savestate_list_tab]

# delete_savestate
set_help_text delete_savestate \
{delete_savestate [<name>]

Delete a previously created savestate.

See also 'savestate', 'loadstate', 'list_savestates'.
}
set_tabcompletion_proc delete_savestate [namespace code savestate_tab]

namespace export savestate
namespace export loadstate
namespace export delete_savestate
namespace export list_savestates

} ;# namespace savestate

namespace import savestate::*
