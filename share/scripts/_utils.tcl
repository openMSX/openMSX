# Several utility procs for usage in other scripts
# don't export anything, just use it from the namespace,
# because these scripts aren't useful for console users
# and should therefore not be exported to the global
# namespace.
#
# These procs are not specific to anything special,
# they could be useful in any script.
#
# Born to prevent duplication between scripts for common stuff.

namespace eval utils {

proc get_machine_display_name {{machineid ""}} {
	if {$machineid eq ""} {
		set machineid [machine]
	}
	if {$machineid eq ""} {
		return "<none>"
	}
	set config_name [${machineid}::machine_info config_name]
	return [get_machine_display_name_by_config_name $config_name]
}

proc get_machine_display_name_by_config_name {config_name} {
	return [get_display_name_by_config_name $config_name "machines"]
}

proc get_extension_display_name_by_config_name {config_name} {
	return [get_display_name_by_config_name $config_name "extensions"]
}

proc get_display_name_by_config_name {config_name type} {
	if {[catch {
		set names [openmsx_info $type $config_name]
		if {$type eq "machines"} {
			set keylist [list "manufacturer" "code"]
		} elseif {$type eq "extensions"} {
			set keylist [list "manufacturer" "code" "name"]
		} else {
			error "Unsupported type: $type"
		}
		set arglist [list]
		foreach key $keylist {
			if [dict exists $names $key] {
				set arg [dict get $names $key]
				if {$arg ne ""} {
					lappend arglist $arg
				}
			}
		}
		set result [join $arglist]
		# fallback if this didn't give useful results:
		if {$result eq ""} {
			set result $config_name
		}
	}]} {
		# hmm, XML file probably broken. Fallback:
		set result "$config_name (CORRUPT)"
	}
	return $result
}

proc get_machine_time {{machineid ""}} {
	if {$machineid eq ""} {
		set machineid [machine]
	}
	set err [catch {set mtime [${machineid}::machine_info time]}]
	if {$err} {
		return ""
	}
	return [format_time $mtime]
}

proc format_time {time} {
	format "%02d:%02d:%02d" [expr {int($time / 3600)}] [expr {int($time / 60) % 60}] [expr {int($time) % 60}]
}

proc format_time_subseconds {time} {
	format "%02d:%02d.%02d" [expr {int($time / 60)}] [expr {int($time) % 60}] [expr {int(fmod($time,1) * 100)}]
}

proc format_time_hours_and_subseconds {time} {
	format "%02d:%02d:%02d.%02d" [expr {int($time / 3600)}] [expr {int($time / 60) % 60}] [expr {int($time) % 60}] [expr {int(fmod($time,1) * 100)}]
}

proc get_machine_total_ram {{machineid ""}} {
	if {$machineid eq ""} {
		set machineid [machine]
	}
	set result 0
	foreach device [${machineid}::debug list] {
		if {[${machineid}::debug desc $device] in [list "memory mapper" "ram"]} {
			incr result [${machineid}::debug size $device]
		}
	}
	return $result
}

proc get_ordered_machine_list {} {
	lsort -dictionary [list_machines]
}

proc get_random_number {max} {
	expr {floor(rand() * $max)}
}

proc clip {min max val} {
	expr {($val < $min) ? $min : (($val > $max) ? $max : $val)}
}

# Provides file completion.
proc file_completion {args} {
	set my_arg [lindex $args end]
	lassign [gather_directory_contents $my_arg] \
		dirstr tail dir_candidates file_candidates
	set entries [list {*}$dir_candidates {*}$file_candidates]
	set result $entries
	if {[string length $tail]} {
		set result ""
		foreach e $entries {
			if {![string first $tail $e]} { lappend result $e }
		}
	}
	if {![llength $result]} {
		puts "No file matches: $my_arg"
		return [list ---rewrite $my_arg]
	} elseif {1 == [llength $result]} {
		return [make_rewrite_or_done $dirstr$result]
	}
	set result_text ""
	foreach e $result { append result_text "$e  " }
	puts $result_text
	set pos [string length $tail]
	while {true} {
		set c [string index [lindex $result 0] $pos]
		if {$c eq ""} { break }
		foreach e $result {
			if {[string index $e $pos] ne $c} {
				set pos -1
				break
			}
		}
		if {$pos == -1} { break }
		append my_arg $c
		incr pos
	}
	return [list ---rewrite $my_arg]
}

# Another file completion.
proc file_completion_by_number {args} {
	set my_arg [lindex $args end]
	lassign [gather_directory_contents $my_arg] \
		dirstr tail dir_candidates file_candidates
	set result [list {*}$dir_candidates {*}$file_candidates]
	if {![llength $result]} {
		puts "No file matches: $my_arg"
		return [list ---rewrite $my_arg]
	} elseif {1 == [llength $result]} {
		set tail 0
	}
	if {![string is integer $tail]} {
		puts "Give the number for last component of the path."
		puts [make_numbered_list_str $result]
		return [list ---rewrite $dirstr]
	} elseif {$tail < 0 || [llength $result] <= $tail} {
		puts "Give the number from 0 to [expr {[llength $result]-1}]"
		puts [make_numbered_list_str $result]
		return [list ---rewrite $dirstr]
	}
	set p "$dirstr[lindex $result $tail]"
	set r [make_rewrite_or_done $p]
	if {![string first $r "---rewrite"]} {
		lassign [gather_directory_contents $p] \
			dmy1 dmy1 new_dir_candidates new_file_candidates
		set new_result [list {*}$new_dir_candidates {*}$new_file_candidates]
		puts [make_numbered_list_str $new_result]
	}
	return $r
}

proc make_numbered_list_str {entries} {
	set cnt 0
	set result ""
	foreach e $entries { set result "$result$cnt:$e  "; incr cnt }
	return $result
}

proc make_rewrite_or_done {path} {
	set is_dir [file isdirectory $path]
	return [list [expr {$is_dir ? "---rewrite": "---done"}] $path]
}

proc gather_directory_contents {path} {
	lassign [split_dir_and_tail $path] dirstr tail
	set dir_candidates ""
	set file_candidates ""
	foreach i [glob -nocomplain -path $dirstr *] {
		set tail_i [string range $i [string length $dirstr] end]
		if {[file isdirectory $i]} {
			lappend dir_candidates $tail_i/
		} else {
			lappend file_candidates $tail_i
		}
	}
	set dir_candidates [lsort -dictionary $dir_candidates]
	set file_candidates [lsort -dictionary $file_candidates]
	return [list $dirstr $tail $dir_candidates $file_candidates]
}

#
# Return [list <dirname> <tail>] from given path.
# <dirname> never be empty string and always ends with "/".
# When given path ends with "/" and the path is directory,
# <tail> is always empty string. If not, the tail won't be
# empty string and not ends with "/".
#
proc split_dir_and_tail {path} {
	if {$path eq "" || $path eq "." } { set $path ./ }
	if {$path eq ".."} { set $path ../ }
	set dirstr ""
	set splitted [file split $path]
	foreach e [lrange $splitted 0 end-1] {
		append dirstr $e
		if {[string index $e end] ne "/"} { append dirstr / }
	}
	set tail [lindex $splitted end]        ; # only few case ends with "/".
	if {[string index $path end] eq "/"} { ; # so check with $path, not $tail.
		if {[file isdirectory $path]} {
			set dirstr $path
			set tail ""
		} else {
			set tail [string range $tail 0 end-1]
		}
	}
	return [list $dirstr $tail]
}

# Replaces characters that are invalid in file names on the host OS or
# file system by underscores.
if {$::tcl_platform(platform) eq "windows"} {
	variable _filename_clean_disallowed {[\x00-\x1f\x7f/\\?*:|"<>+\[\]]}
} else {
	# UNIX does allow 0x01-0x1f and 0x7f, but we consider them undesirable.
	variable _filename_clean_disallowed {[\x00-\x1f\x7f/]}
}
proc filename_clean {path} {
	variable _filename_clean_disallowed
	return [regsub -all $_filename_clean_disallowed $path _]
}

# Gets you a filename with numbers in it to avoid overwriting
# an existing file with the same name. Especially useful for
# cases with automatically generated filenames
proc get_next_numbered_filename {directory prefix suffix} {
	set pattern "${prefix}\[0-9\]\[0-9\]\[0-9\]\[0-9\]${suffix}"
	set files [glob -directory $directory -tails -nocomplain $pattern]
	if {[llength $files] == 0} {
		set num "0001"
	} else {
		set name [lindex [lsort $files] end]
		scan [string range $name [string length $prefix] end] "%4d" n
		if {$n == 9999} {
			error "Can't create new filename"
		}
		set num [format "%04d" [expr {$n + 1}]]
	}
	file join $directory "${prefix}${num}${suffix}"
}

namespace export get_machine_display_name
namespace export get_machine_display_name_by_config_name
namespace export get_extension_display_name_by_config_name
namespace export get_display_name_by_config_name
namespace export get_machine_time
namespace export format_time
namespace export get_machine_total_ram
namespace export get_ordered_machine_list
namespace export get_random_number
namespace export clip
namespace export file_completion
namespace export file_completion_by_number
namespace export filename_clean
namespace export get_next_numbered_filename

} ;# namespace utils

# Don't import in global namespace, these are only useful in other scripts.
