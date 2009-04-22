namespace eval save_debuggable {

set_help_text save_debuggable \
{Save a debuggable to file. Examples of debuggables are memory, VRAM, ...
Use 'debug list' to get a complete list of all debuggables.

Usage:
  save_debuggable VRAM vram.raw    Save the content of the MSX VRAM to a file
                                   called 'vram.raw'
}
proc save_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set data [debug read_block $debuggable 0 $size]
	set file [open $filename "WRONLY CREAT TRUNC"]
	fconfigure $file -translation binary -buffersize $size
	puts -nonewline $file $data
	close $file
}

set_help_text load_debuggable \
{Load a raw data file into a certain debuggable (see also save_debuggable).
Note that saving and reloading the same data again does not always bring the
MSX in the same state (e.g. the subslotregister). Use 'savestate' and 'loadstate'
if you want that.
}
proc load_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set file [open $filename "RDONLY"]
	fconfigure $file -translation binary -buffersize $size
	set data [read $file]
	close $file
	debug write_block $debuggable 0 $data
}

set_tabcompletion_proc save_debuggable [namespace code tab_loadsave_debuggable]
set_tabcompletion_proc load_debuggable [namespace code tab_loadsave_debuggable]
proc tab_loadsave_debuggable { args } {
	if {[llength $args] == 2} {
		return [debug list]
	}
}

# TODO remove these two procs?
#  They were meant as a very quick-and-dirty savestate mechanism, but it
#  doesn't work (e.g. because of subslot register)
proc save_all { directory } {
	puts stderr "This command ('save_all') has been deprecated (and may be removed in a future release). In contrast to what you might think, it cannot be used to save the machine state. You probably just want to use 'save_state' instead!"
	foreach debuggable [debug list] {
		save_debuggable $debuggable ${directory}/${debuggable}.sav
	}
}
proc load_all { directory } {
	puts stderr "This command ('load_all') has been deprecated (and may be removed in a future release). In contrast to what you might think, it cannot be used to load the machine state. You probably just want to use 'load_state' instead!"
	foreach debuggable [debug list] {
		load_debuggable $debuggable ${directory}/${debuggable}.sav
	}
}

# for backwards compatibility
proc vramdump { { filename "vramdump"} } {
	#puts stderr "This command ('vramdump') has been deprecated (and may be removed in a future release), please use 'save_debuggable VRAM' instead!"
	# I prefer to keep this command as a convenience shortcut
	save_debuggable "VRAM" $filename
}

namespace export save_debuggable
namespace export load_debuggable
namespace export save_all
namespace export load_all
namespace export vramdump

} ;# namespace save_debuggable

namespace import save_debuggable::*
