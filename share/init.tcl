# $Id$

# evaluate in internal openmsx namespace
namespace eval openmsx {

variable init_tcl_executed false
variable tabcompletion_proc_sensitive
variable tabcompletion_proc_insensitive
variable help_text
variable help_proc

# Only execute this script once. Below we source other Tcl script,
# so this makes sure we don't get in an infinte loop.
if $init_tcl_executed return
set init_tcl_executed true

# internal proc to make help function available to Tcl procs
proc help { args } {
	variable help_text
	variable help_proc

	set command [lindex $args 0]
	if [info exists help_proc($command)] {
		return [namespace eval :: $help_proc($command) $args]
	} elseif [info exists help_text($command)] {
		return $help_text($command)
	} elseif {[info commands $command] ne ""} {
		error "No help for command: $command"
	} else {
		error "Unknown command: $command"
	}
}
proc set_help_text { command help } {
	variable help_text
	set help_text($command) $help
}
set_help_text set_help_text \
{Associate a help-text with a Tcl proc. This is normally only used in Tcl scripts.}

# internal proc to make tabcompletion available to Tcl procs
proc tabcompletion { args } {
	variable tabcompletion_proc_sensitive
	variable tabcompletion_proc_insensitive

	set command [lindex $args 0]
	set result ""
	if [info exists tabcompletion_proc_sensitive($command)] {
		set result [namespace eval :: $tabcompletion_proc_sensitive($command) $args]
		lappend result true
	} elseif [info exists tabcompletion_proc_insensitive($command)] {
		set result [namespace eval :: $tabcompletion_proc_insensitive($command) $args]
		lappend result false
	}
	return $result
}
proc set_tabcompletion_proc { command proc {case_sensitive true} } {
	variable tabcompletion_proc_sensitive
	variable tabcompletion_proc_insensitive
	if $case_sensitive {
		set tabcompletion_proc_sensitive($command) $proc
	} else {
		set tabcompletion_proc_insensitive($command) $proc
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

namespace export set_help_text
namespace export set_tabcompletion_proc
namespace export data_file

} ;# namespace openmsx

namespace import openmsx::*

namespace eval openmsx {
# Source all .tcl files in user and system scripts directory. Prefer
# the version in the user directory in case a script exists in both
set user_scripts [glob -dir $env(OPENMSX_USER_DATA)/scripts -tails -nocomplain *.tcl]
set system_scripts [glob -dir $env(OPENMSX_SYSTEM_DATA)/scripts -tails -nocomplain *.tcl]
foreach script [lsort -unique [concat $user_scripts $system_scripts]] {
	set script [data_file scripts/$script]
	if {[catch {namespace eval :: "source $script"}]} {
		puts stderr "Error while executing $script\n$errorInfo"
	}
}

} ;# namespace openmsx
