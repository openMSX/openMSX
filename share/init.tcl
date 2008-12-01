# $Id$

# Only execute this script once. Below we source other Tcl script,
# so this makes sure we don't get in an infinte loop.
if [info exists ::__init_tcl_executed] return
set ::__init_tcl_executed true

# internal proc to make help function available to Tcl procs
proc __help { args } {
	set command [lindex $args 0]
	if [info exists ::__help_proc($command)] {
		return [eval $::__help_proc($command) $args]
	} elseif [info exists ::__help_text($command)] {
		return $::__help_text($command)
	} elseif {[info commands $command] ne ""} {
		error "No help for command: $command"
	} else {
		error "Unknown command: $command"
	}
}
proc set_help_text { command help } {
	set ::__help_text($command) $help
}
set_help_text set_help_text \
{Associate a help-text with a Tcl proc. This is normally only used in Tcl scripts.}

# internal proc to make tabcompletion available to Tcl procs
proc __tabcompletion { args } {
	set command [lindex $args 0]
	set result ""
	if [info exists ::__tabcompletion_proc_sensitive($command)] {
		set result [eval $::__tabcompletion_proc_sensitive($command) $args]
		lappend result true
	} elseif [info exists ::__tabcompletion_proc_insensitive($command)] {
		set result [eval $::__tabcompletion_proc_insensitive($command) $args]
		lappend result false
	}
	return $result
}
proc set_tabcompletion_proc { command proc {case_sensitive true} } {
	if $case_sensitive {
		set ::__tabcompletion_proc_sensitive($command) $proc
	} else {
		set ::__tabcompletion_proc_insensitive($command) $proc
	}
}
set_help_text set_tabcompletion_proc \
{Provide a way to do tab-completion for a certain Tcl proc. For details look at the numerous examples in the share/scripts directory. This is normally only used in Tcl scripts.}

set_help_text data_file \
"Resolve data file. First try user directory, if the file doesn't exist
there try the system directory."
proc data_file { file } {
	global env
	set user_file $env(OPENMSX_USER_DATA)/$file
	if [file exists $user_file] { return $user_file }
	return $env(OPENMSX_SYSTEM_DATA)/$file
}

# Source all .tcl files in user and system scripts directory. Prefer
# the version in the user directory in case a script exists in both
set __user_scripts [glob -dir $env(OPENMSX_USER_DATA)/scripts -tails -nocomplain *.tcl]
set __system_scripts [glob -dir $env(OPENMSX_SYSTEM_DATA)/scripts -tails -nocomplain *.tcl]
foreach __script [lsort -unique [concat $__user_scripts $__system_scripts]] {
	set __script [data_file scripts/$__script]
	if {[catch {source $__script}]} {
		puts stderr "Error while executing $__script\n$errorInfo"
	}
}
