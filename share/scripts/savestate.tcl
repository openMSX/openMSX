# convenience wrappers around the low level savestate commands

proc __savestate_common { name } {
	uplevel {
		if {$name == ""} { set name "quicksave" }
		set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
		set fullname [file join $directory ${name}.xml.gz]
		set png [file join $directory ${name}.png]
	}
}

#TODO rename
proc my_save { {name ""} } {
	__savestate_common $name
	catch { screenshot $png }
	set currentID [machine]
	savestate $currentID $fullname
	return $name
}

#TODO rename
proc my_load { {name ""} } {
	__savestate_common $name
	set newID [loadstate $fullname]
	set currentID [machine]
	delete_machine $currentID
	activate_machine $newID
	return $name
}

proc list_savestates {} {
	set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
	set result [list]
	foreach f [glob -tails -directory $directory *.xml.gz] {
		lappend result [file rootname [file rootname $f]]
	}
	return [lsort $result]
}

proc delete_savestate { {name ""} } {
	__savestate_common $name
	catch { file delete -- $fullname }
	catch { file delete -- $png }
	return ""
}

proc __savestate_tab { args } {
	return [list_savestates]
}

# my_save
set_help_text my_save \
{my_save [<name>]

Create a snapshot of the current emulated MSX machine.

Optionally you can specify a name for the savestate. If you omit this the default name 'quicksave' will be taken.

See also 'my_load', 'list_savestates', 'delete_savestate'.
}
set_tabcompletion_proc my_save __savestate_tab

# my_load
set_help_text my_load \
{my_load [<name>]

Restore a previously created savestate.

You can specify the name of the savestate that should be loaded. If you omit this name, the default savestate will be loaded.

See also 'my_save', 'list_savestates', 'delete_savestate'.
}
set_tabcompletion_proc my_load __savestate_tab

# list_savestates
set_help_text list_savestates \
{list_savestates

Return the names of all previously created savestates.

See also 'my_save', 'my_load', 'delete_savestate'.
}

# delete_savestate
set_help_text my_load \
{delete_savestate [<name>]

Delete a previously created savestate.

See also 'my_save', 'my_load', 'list_savestates'.
}
set_tabcompletion_proc delete_savestate __savestate_tab
