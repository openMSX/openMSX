proc save_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set data [debug read_block $debuggable 0 $size]
	set file [open $filename "WRONLY CREAT TRUNC"]
	fconfigure $file -translation binary -buffersize $size
	puts -nonewline $file $data
	close $file
}
proc load_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set file [open $filename "RDONLY"]
	fconfigure $file -translation binary -buffersize $size
	set data [read $file]
	close $file
	debug write_block $debuggable 0 $data
}

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

