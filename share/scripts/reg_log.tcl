
set_help_text reg_log \
{TODO
}

set_tabcompletion_proc reg_log tab_reg_log
proc tab_reg_log { args } {
	switch [llength $args] {
		2 { return "record play stop" }
		3 { return [debug list] }
	}
}

proc reg_log { subcommand debuggable {filename ""} } {
	if {$filename == ""} { set filename ${debuggable}.log }
	switch $subcommand {
		"record" { return [__reg_log_record $debuggable $filename] }
		"play"   { return [__reg_log_play $debuggable $filename] }
		"stop"   { return [__reg_log_stop $debuggable] }
		default  { error "bad option \"$subcommand\": must be start stop" }
	}
}

proc __reg_log_check { debuggable } {
	if {[lsearch [debug list] $debuggable] == -1} {
		error "No such debuggable: $debuggable"
	}
}

proc __reg_log_record { debuggable filename } {
	global __reg_log_file
	__reg_log_check $debuggable
	__reg_log_stop $debuggable
	set __reg_log_file($debuggable) [open $filename {WRONLY TRUNC CREAT}]
	__do_reg_record $debuggable
	return ""
}

proc __reg_log_play { debuggable filename } {
	global __reg_log_data
	__reg_log_check $debuggable
	__reg_log_stop $debuggable
	set file [open $filename RDONLY]
	set __reg_log_data($debuggable) [split [read $file] \n]
	close $file
	__do_reg_play $debuggable
	return ""
}

proc __reg_log_stop { debuggable } {
	global __reg_log_file __reg_log_data
	if [info exists __reg_log_file($debuggable)] {
		close $__reg_log_file($debuggable)
		unset __reg_log_file($debuggable)
	}
	if [info exists __reg_log_data($debuggable)] {
		unset __reg_log_data($debuggable)
	}
	return ""
}

proc __do_reg_record { debuggable } {
	global __reg_log_file
	if ![info exists __reg_log_file($debuggable)] return
	set size [debug size $debuggable]
	for {set i 0} {$i < $size} {incr i} {
		puts -nonewline $__reg_log_file($debuggable) "[debug read $debuggable $i] "
	}
	puts $__reg_log_file($debuggable) ""  ;#newline
	after frame [list __do_reg_record $debuggable]
}

proc __do_reg_play { debuggable } {
	global __reg_log_data
	if ![info exists __reg_log_data($debuggable)] return
	set reg 0
	foreach val [lindex $__reg_log_data($debuggable) 0] {
		debug write $debuggable $reg $val
		incr reg
	}
	set __reg_log_data($debuggable) [lrange $__reg_log_data($debuggable) 1 end]
	if {[llength $__reg_log_data($debuggable)] > 0} {
		after frame [list __do_reg_play $debuggable]
	}
}
