# $Id$

# Backwards compatibility commands
proc decr { var { num 1 } } {
	uplevel incr $var [expr -$num]
}
proc restoredefault { var } {
	uplevel unset $var
}
proc alias { cmd body } {
	proc $cmd {} $body
}

# Resolve data files. First try user directory, if the file doesn't exist
# there try the system direcectory
proc data_file { file } {
	global env
	set user_file $env(OPENMSX_USER_DATA)/$file
	if [file exists $user_file] { return $user_file }
	return $env(OPENMSX_SYSTEM_DATA)/$file
}

# Source all .tcl files in user and system scripts directory. Prefer
# the version in the user directory in case a script exists in both
set user_scripts [glob -dir $env(OPENMSX_USER_DATA)/scripts -tails -nocomplain *.tcl]
set system_scripts [glob -dir $env(OPENMSX_SYSTEM_DATA)/scripts -tails -nocomplain *.tcl]
foreach script [lsort -unique [concat $user_scripts $system_scripts]] {
	source [data_file scripts/$script]
}

# Execute the init.tcl file in the user's directory (if it exists)
set user_init_tcl $env(OPENMSX_USER_DATA)/init.tcl
if [file exists $user_init_tcl] { source $user_init_tcl }
