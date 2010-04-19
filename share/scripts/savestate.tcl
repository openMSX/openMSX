# convenience wrappers around the low level savestate commands

namespace eval savestate {

proc savestate_common { name } {
	uplevel {
		if {$name == ""} { set name "quicksave" }
		set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
		set fullname [file join $directory ${name}.xml.gz]
		set png [file join $directory ${name}.png]
	}
}

proc savestate { {name ""} } {
	savestate_common $name
	file mkdir $directory
	if {[catch { screenshot -raw -doublesize $png }]} {
		# some renderers don't support msx-only screenshots
		if {[catch { screenshot $png }]} {
			# even this failed, but (try to) remove old screenshot
			# to avoid confusion
			catch { file delete $png }
		}
	}
	set currentID [machine]
	store_machine $currentID $fullname
	return $name
}

proc loadstate { {name ""} } {
	savestate_common $name
	set newID [restore_machine $fullname]
	set currentID [machine]
	if {$currentID != ""} { delete_machine $currentID }
	activate_machine $newID
	return $name
}

# helper proc to get the raw savestate info
proc list_savestates_raw {} {
	set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
	set results [list]
	foreach f [glob -tails -directory $directory -nocomplain *.xml.gz] {
		set name [file rootname [file rootname $f]]
		set fullname [file join $directory $f]
		set filetime [file mtime $fullname]
		lappend results [list $name $filetime]
	}
	return $results
}

proc list_savestates { args } {
	set sort_key 0
	set long_format false
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
		"-l" {
			if {[info commands clock] != ""} {
				set long_format true
			} else {
				error "Sorry, long format not supported on this system (missing clock.tcl)"
			}
			set args [lrange $args 1 end]
		}
		"default" {
			error "Invalid option: [lindex $args 0]"
		}
		}
	}

	set sorted_sublists [lsort ${sort_option} ${sort_order} -index $sort_key [list_savestates_raw]]
	
	if {!$long_format} {
		set sorted_result [list]
		foreach sublist $sorted_sublists {lappend sorted_result [lindex $sublist 0]}
		return $sorted_result
	} else {
		set stringres ""
		foreach sublist $sorted_sublists {
			append stringres [format "%-[expr round(${::consolecolumns} / 2)]s %s\n" [lindex $sublist 0] [clock format [lindex $sublist 1] -format "%a %b %d %Y - %H:%M:%S" ]]
		}
		return $stringres
	}
}

proc delete_savestate { {name ""} } {
	savestate_common $name
	catch { file delete -- $fullname }
	catch { file delete -- $png }
	return ""
}

proc savestate_tab { args } {
	return [list_savestates]
}

proc savestate_list_tab { args } {
	return {"-l" "-t"}
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
  -l      long formatting, showing date of savestates

Note: the -l option is not available on all systems.

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


# keybindings
if {$tcl_platform(os) == "Darwin"} {
	bind_default META+S savestate
	bind_default META+R loadstate
} else {
	bind_default ALT+F8 savestate
	bind_default ALT+F7 loadstate
}

namespace export savestate
namespace export loadstate
namespace export delete_savestate
namespace export list_savestates

} ;# namespace savestate

namespace import savestate::*
