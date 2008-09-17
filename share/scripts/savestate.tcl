
proc __savestate_name { name } {
	uplevel {
		if {$name == ""} { set name "quicksave" }
		set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
		set fullname [file join $directory ${name}.xml.gz]
	}
}

#TODO rename
proc my_save { {name ""} } {
	__savestate_name $name
	set png [file join $directory ${name}.png]
	catch { screenshot $png }
	set currentID [machine]
	savestate $currentID $fullname
	return $name
}

#TODO rename
proc my_load { {name ""} } {
	__savestate_name $name
	set newID [loadstate $fullname]
	set currentID [machine]
	delete_machine $currentID
	activate_machine $newID
}

set_help_text my_save \
{my_save [<name>]

TODO

}

set_help_text my_load \
{my_load [<name>]

TODO

}

set_tabcompletion_proc my_save tab_savestate
set_tabcompletion_proc my_load tab_savestate
proc tab_savestate { args } {
	set directory [file normalize $::env(OPENMSX_USER_DATA)/../savestates]
	set result [list]
	foreach f [glob -tails -directory $directory *.xml.gz] {
		lappend result [file rootname [file rootname $f]]
	}
	return $result
}
