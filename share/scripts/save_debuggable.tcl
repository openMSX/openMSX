proc save_debuggable {debuggable filename} {
	set size [debug size $debuggable]
	set data [debug read_block $debuggable 0 $size]
	set file [open $filename "WRONLY CREAT TRUNC"]
	fconfigure $file -translation binary -buffersize $size
	puts -nonewline $file $data
	close $file
}

# for backwards compatibility
proc vramdump { { filename "vramdump"} } {
	save_debuggable vram $filename
}
