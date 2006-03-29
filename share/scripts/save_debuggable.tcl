set_help_text save_debuggable \
{Save a debuggable to file. Examples of debuggables are memory, vram, ...
Use 'debug list' to get a complete list of all debuggables.

Usage:
  save_debuggable VRAM vramdump    Save the content of the MSX VRAM to a file
                                   called vramdump
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
MSX in the same state (e.g. the subslotregister)
}
proc load_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set file [open $filename "RDONLY"]
	fconfigure $file -translation binary -buffersize $size
	set data [read $file]
	close $file
	debug write_block $debuggable 0 $data
}

# TODO remove these two procs?
#  They were ment as a very quick-and-dirty savestate mechanism, but it
#  doesn't work (e.g. because of subslot register)
proc save_all { directory } {
	foreach debuggable [debug list] {
		save_debuggable $debuggable ${directory}/${debuggable}.sav
	}
}
proc load_all { directory } {
	foreach debuggable [debug list] {
		load_debuggable $debuggable ${directory}/${debuggable}.sav
	}
}

# for backwards compatibility
proc vramdump { { filename "vramdump"} } {
	save_debuggable "VRAM" $filename
}

