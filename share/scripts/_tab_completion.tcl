namespace eval tab_completion {

variable alternation_trigger ","
variable alternation_markers "0123456789abcdefghijklmnopqrstuvwxyz"
variable alternation_regex "${alternation_trigger}(\[0-9a-z\]{1,4})\$"
variable alternation_conflicts_regex "\\A${alternation_trigger}(\[0-9a-z\])"

proc set_tab_alternation_trigger {trigger} {
	variable alternation_trigger
	if {$trigger eq ""} { return $alternation_trigger }
	variable alternation_regex
	variable alternation_conflicts_regex
	set alternation_trigger $trigger
	set alternation_regex "${trigger}(\[0-9a-z\]{1,4})\$"
	set alternation_conflicts_regex "\\A${trigger}(\[0-9a-z\])"
	return $trigger
}

set_help_text set_tab_alternation_trigger \
{Usage: set_tab_alternation_trigger ?character?
Get or change the trigger of TAB-key alternation for non-ASCII.
Default is "," (comma). Use both Tcl and regular expression safe character.
(This help is only available after once TAB pressed.)}

#
# BE CAREFUL TO RENAME OR/AND REPLACE! See 'src/command/Completer.cc'
# 
proc format_to_columns_from_cpp {input} {
	return [format_to_columns input]
}

variable generic_possible_values

#
# Tab completion procedure for generic usage.
# BE CAREFUL TO RENAME OR/AND REPLACE! See 'src/command/Completer.cc',
# 'share/init.tcl' and 'share/scripts/lazy.tcl'.
#
# 'case_sensitive' is only used for sort `possible_values', and if you want
# sorting, supply exact 'true' or 'false'. Otherwise no sorting and
# unique-ing performed.
#
proc generic_tab_completion {case_sensitive possible_values args} {
	variable generic_possible_values
	set cb [namespace code get_generic_possibilities]
	switch $case_sensitive {
		true - false {
			set generic_possible_values [ \
				tc_sort_by_cpp $case_sensitive {*}$possible_values]
		}
		default {
			set generic_possible_values $possible_values
		}
	}
	set cmn_args [list $case_sensitive $cb args]
	set path_related [list {} {{}} false]

	return [common_for_tab_completion cmn_args path_related]
}

proc get_generic_possibilities {filter_arg dmy1 dmy2 dmy3} {
	variable generic_possible_values
	return [list {} $filter_arg $generic_possible_values]
}

#
# Tab completion procedure for file path.
# BE CAREFUL TO RENAME OR/AND REPLACE! See 'src/command/Completer.cc',
# 'share/scripts/_utils.tcl' and 'share/scripts/lazy.tcl'
#
proc file_tab_completion {case_sensitive possible_parents extras args} {
	set cb [namespace code get_file_path_possibilities]
	set cmn_args [list $case_sensitive $cb args]
	set path_related [list $extras $possible_parents true]

	return [common_for_tab_completion cmn_args path_related]
}

proc get_file_path_possibilities { \
	filter_arg case_sensitive extras possible_parents \
} {
	if {![llength $possible_parents]} { lappend possible_parents ./ }
	set target_full_paths ""
	foreach p $possible_parents {
		# [file join] expands "." to current working directory,
		# and drop previous path when a following one is absolute path.
		set fp [weak_normalize_path . $p]
		if {[string index $fp end] ne "/"} { append fp / }
		lappend target_full_paths $fp
	}
	set child_dirs ""
	set child_files ""

	# Make clear to point directory by "/" for some dot paths.
	if {$filter_arg eq "" || $filter_arg eq "." } { set filter_arg ./ }
	if {$filter_arg eq ".." || [regexp /\\.\\.\$ $filter_arg]} {
		append filter_arg
	}

	# Collect entries of target parent directories.
	if {[file pathtype $filter_arg] ne "relative"} {
		lassign [gather_directory_contents $filter_arg] \
			dirstr tail child_dirs child_files
	} else {
		foreach p $target_full_paths {
			set full_path [weak_normalize_path $p $filter_arg]
			# Note: because $filter_arg is not empty, $full_path never be empty.
			if {[string index $filter_arg end] eq "/" && \
				[string index $full_path end] ne "/" \
			} {
				append full_path /
			}
			lassign [gather_directory_contents $full_path] \
				dirstr tail child_dirs_for_p child_files_for_p
			lappend child_dirs {*}$child_dirs_for_p
			lappend child_files {*}$child_files_for_p
		}
		# Only one time fixing $dirstr is enough,
		# Because it should be relative path from $target_full_paths.
		if {![string first $p $dirstr]} {
			set po [expr {[string length $p] - 1}]
			for {} {$po >= 0 && [string index $p $po] ne "/"} {incr po -1} { }
			set dirstr [string range $dirstr [expr {$po + 1}] end]
		} else {
			set pos_last_common_separator 0
			for {set cnt 0} {true} {incr cnt} {
				set pc [string index $p $cnt]
				set dc [string index $dirstr $cnt]
				if {$pc eq $dc} {
					if {$pc eq "/"} { set pos_last_common_separator $cnt }
					continue
				}
				set plcs [expr {$pos_last_common_separator + 1}]
				set p_unmatch [string range $p $plcs end]
				set d_unmatch [string range $dirstr $plcs end]
				set up_dir_cnt [llength [file split $p_unmatch]]
				set dirstr "[string repeat ../ $up_dir_cnt]$d_unmatch"
				break
			}
		}
	}

	set child_dirs [tc_sort_by_cpp $case_sensitive {*}$child_dirs]
	set child_files [tc_sort_by_cpp $case_sensitive {*}$child_files]
	return [list $dirstr $tail [list {*}$child_dirs {*}$child_files {*}$extras]]
}

proc common_for_tab_completion {cmn_args_name path_related_name} {
	upvar $cmn_args_name cmn_args
	upvar $path_related_name path_related
	lassign $cmn_args case_sensitive callback args_name
	upvar $args_name args
	lassign $path_related extras possible_parents

	# filter entries by the user already typed.
	lassign [get_about_last_arg args] filter_arg alter_given last_arg
	lassign [{*}$callback \
		$filter_arg $case_sensitive $extras $possible_parents] \
		dirstr tail possibilities
	set entries [filter_entries $tail possibilities]

	if {$alter_given ne ""} {
		gather_non_ascii_targets $tail entries dict_to_alter
		set alter_keys [dict keys $dict_to_alter]
		if {[lsearch -exact $alter_keys $alter_given] != -1} {
			lassign [dict get $dict_to_alter $alter_given] result count
			set cont [expr {1 < $count}]
			return [make_cont_or_done $dirstr$result path_related $cont]
		}
		# If $alter_given is just 'look like but not intend to',
		# treat it as normal string.
		set filter_arg $last_arg
		set alter_given ""
		lassign [{*}$callback \
			$filter_arg $case_sensitive $extras $possible_parents] \
			dirstr tail possibilities
		set entries [filter_entries $tail possibilities]
	}

	# When the count of entries are zero or one, finish here.
	if {![llength $entries]} {
		puts "--- No matches found: $last_arg"
		return [list ---cont $filter_arg ---]
	} elseif {1 == [llength $entries]} {
		set e [lindex $entries 0] ; # {any text} to "any text". Required.
		return [make_cont_or_done $dirstr$e path_related false]
	}

	# Otherwise, show what the lastest console input before update.
	puts "--- $last_arg"
	gather_non_ascii_targets $tail entries dict_to_alter
	set alter_keys [dict keys $dict_to_alter]	
	proceed_common_part $tail entries filter_arg
	show_possibilities entries
	if {[llength $alter_keys]} {
		set selectable_strs ""
		variable alternation_trigger
		set atg $alternation_trigger
		foreach n $alter_keys {
			lassign [dict get $dict_to_alter $n] entry count
			set s "\[$atg$n\]$entry"
			if {$count > 1} { append s "... <<$count>>" }
			lappend selectable_strs $s
		}
		show_possibilities selectable_strs
		lassign [lrange $alter_keys 0 2] s0 s1 s2
		switch [llength $selectable_strs] {
			1 { set s $atg$s0 }
			2 { set s "$atg$s0 or $atg$s1" } 
			3 { set s "$atg$s0 $atg$s1 or $atg$s2" }
			default { set s "$atg$s0 $atg$s1 $atg$s2 ..." }
		}
		puts "You can use $s to select associated string."
	}
	return [list ---cont $filter_arg ---]
}

proc get_about_last_arg {args_name} {
	upvar $args_name args
	# $args should have two or more elements here.
	# (When only one or zero, this procedure is not called.)
	set last_arg [lindex $args end]

	# Check alternation mode.
	variable alternation_regex
	set filter_arg $last_arg
	if {[regexp ${alternation_regex} $last_arg m0 alter_given]} {
		set filter_arg [string range $last_arg 0 end-[string length $m0]]
	} else {
		set alter_given ""
	}
	return [list $filter_arg $alter_given $last_arg]
}

proc make_cont_or_done {item path_related_name force_cont} {
	upvar $path_related_name path_related
	lassign [lrange $path_related end-1 end] possible_parents is_path
	set cont $force_cont
	if {$is_path} {
		if {!$cont} { set cont [file isdirectory $item] }
		foreach p $possible_parents {
			if {$cont} { break }
			if {[file isdirectory [file join $p $item]]} {
				set cont true
			}
		}
	}
	return [list [expr {$cont ? "---cont": "---done"}] $item ---]
}

proc proceed_common_part {tail result_name our_arg_name} {
	upvar $result_name result
	upvar $our_arg_name our_arg
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
		append our_arg $c
		incr pos
	}
}

proc gather_directory_contents {path} {
	lassign [split_dir_and_tail $path false] dirstr tail
	foreach t {d f} n {dir_candidates file_candidates} sep {"/" ""} {
		set raw [glob -type $t -nocomplain -path $dirstr *]
		set $n ""
		foreach e $raw {
			lappend $n [string range $e [string length $dirstr] end]$sep
		}
	}
	return [list $dirstr $tail $dir_candidates $file_candidates]
}

#
# Do like [file normalize [file join $args]], but don't resolve symbolic
# links like `file normalize`.
#
proc weak_normalize_path {args} {
	if {[llength $args] < 1} {
		# [file join ...] requires at least one name.
		retun $args
	}
	set append_to_last ""
	if {[string index [lindex $args end] end] eq "/"} {
		set append_to_last /
	}
	set joined [file join {*}$args]
	set splitted [file split $joined]
	set components ""
	foreach e $splitted {
		if {$e eq ".."} {
			if {[lsearch -exact {"~" "." ".."} [lindex $components end]]<0} {
				set components [lreplace $components end end]
			} else {
				lappend components $e    
			}
		} elseif {$e eq "." && [llength $components]} {
			# just skip
		} else {
			lappend components $e
		}
	}
	if {[llength $components] < 1} {
		return "$joined$append_to_last"
	}
	return "[file join {*}$components]$append_to_last"
}

#
# Return [list <dirname> <tail>] from given path.
# <dirname> never be empty string and always ends with "/".
# When given path ends with "/", <tail> is empty string.
# If last_separator_is_ignorable is true (as default) and
# when given path is not accesible as directory, the last
# component of given path will split to <tail> and snip last "/".
#
proc split_dir_and_tail {path {last_separator_is_ignorable true}} {
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
		if {!$last_separator_is_ignorable || [file isdirectory $path]} {
			set dirstr $path
			set tail ""
		} else {
			set tail [string range $tail 0 end-1]
		}
	}
	return [list $dirstr $tail]
}

proc filter_entries {tail possibilities_name} {
	upvar $possibilities_name possibilities
	if {![string length $tail]} { return $possibilities }
	set entries ""
	foreach e $possibilities {
		if {![string first $tail $e]} { lappend entries $e }
	}
	return $entries
}

proc show_possibilities {result_name} {
	upvar $result_name result
	foreach l [format_to_columns result] {
		puts $l
	}
}

proc format_to_columns {input_name} {
	upvar $input_name input
	set max_column [tc_get_console_width] ; # see `src/commands/Completer.cc'.
	incr max_column -1
	set i_lengths ""
	foreach e $input { lappend i_lengths [get_mono_width e] }
	set i_size [llength $input]
	set stops ""
	set min_l 1
	set max_l $i_size
	set lines $i_size
	while {$min_l < $max_l} {
		set lines [expr {int(($min_l + $max_l) / 2)}]
		set try_stops ""
		if {[calc_to_make_columns $lines $max_column try_stops i_lengths]} {
			set max_l $lines
			set stops $try_stops
		} else {
			set min_l [expr {$lines + 1}]
			set lines $min_l
		}
	}
	set result [lrange $input 0 [expr {$lines - 1}]]
	set ii_l 0
	set ii $lines
	set col_l 0
	foreach col $stops {
		set diff_col [expr {$col - $col_l}]
		for {set ln_i 0} {$ln_i < $lines && $ii < $i_size} {
			incr ln_i; incr ii; incr ii_l} \
		{
			set ws_s [expr {$diff_col - [lindex $i_lengths $ii_l]}]
			set ws [string repeat " " $ws_s]
			lset result $ln_i [join \
				[list [lindex $result $ln_i] [lindex $input $ii]] $ws]
		}
		set col_l $col
	}
	return $result
}

proc calc_to_make_columns {lines max_column stops_name lengths_name} {
	upvar $stops_name stops
	upvar $lengths_name i_lengths
	set len_i [llength $i_lengths]
	set column 0
	set bi 0
	set ei [expr {$bi + $lines - 1}]
	set stops ""
	while {true} {
		set widest [lindex \
			[lsort -integer [lrange $i_lengths $bi $ei]] end]
		if {$max_column < $column + $widest} { return false }
		incr bi $lines
		if {$len_i <= $bi} { return true }
		incr ei $lines
		incr column $widest
		incr column 2
		lappend stops $column
	}
}

proc gather_non_ascii_targets {tail entries_name o_dict_name} {
	upvar $entries_name entries
	upvar $o_dict_name o_dict

	# Make the dict character-key to entries-value. 
	set compare_starts [string length $tail]
	set targets_dict ""
	set used_ids ""
	variable alternation_conflicts_regex
	set rexp "${alternation_conflicts_regex}|\\A\[\\u0080-\\uFFFF\]" 
	foreach e $entries {
		if {[regexp -start $compare_starts $rexp $e whole_ id_]} {
			if {$id_ eq ""} {
				dict lappend targets_dict $whole_ $e
			} else {
				lappend used_ids $id_
			}
		}
	}
	
	# When no next positions has non-ASCII character, just return here.
	set o_dict ""
	if {![dict size $targets_dict]} { return }

	# As possible as we can, avoid to conflict existing possibilities with
	# alternation expressions.
	variable alternation_markers
	set am $alternation_markers
	set m_len [string length $am]
	set usable_heads ""
	for {set idx 0} {$idx < $m_len} {incr idx} {
		set e [string index $am $idx]
		if {[lsearch $used_ids $e] == -1} { append usable_heads $e }
	}
	if {![string length $usable_heads]} { set usable_heads $am }

	# Append common strings to each non-ASCII characters.
	set longests ""
	dict for {key values} $targets_dict {
		set cnt [llength $values]
		if {$cnt == 1} {
			dict set longests [lindex $values 0] $cnt
			continue
		}
		set s [join [list $tail $key] ""]
		for {set c_idx [string length $s]} {true} {incr c_idx} {
			set t [string index [lindex $values 0] $c_idx]
			if {$t eq ""} { break }
			for {set i 1} {$i < $cnt} {incr i} {
				if {$t ne [string index [lindex $values $i] $c_idx]} {
					set t ""; break
				}
			}
			if {$t eq ""} { break }
			append s $t
		}
		dict set longests $s $cnt
	}

	# Assign alternate expression to non-ASCII next characters.
	# Note: The 3rd or more right character of id cannot be the 1st
	# character of $am. Because that character is hidden as shorter 
	# length id.
	set h_len [string length $usable_heads]
	set head_idx -1
	set sub_idx -1
	set sub_str ""
	dict for {e cnt} $longests {
		incr head_idx
		if {$h_len <= $head_idx} {
			set head_idx 0
			incr sub_idx
			set sub_str ""
			set si $sub_idx
			while {true} {
				set si_mod [expr {$si % $m_len}]
				append sub_str [string index $am $si_mod]
				if {$si_mod == $si} { break }
				set si [expr {int($si / $m_len)}]
			}
		}
		set id_ "[string index $usable_heads $head_idx]$sub_str"
		dict set o_dict $id_ [list $e $cnt]
	}
}

proc get_mono_width {s_name} {
	upvar $s_name s
	variable monospace_width_proc
	# use [list $s] to support when $s have white spaces.
	return [eval $monospace_width_proc [list $s]]
}

variable monospace_width_proc [namespace code monospace_width_proc_default]

set_tabcompletion_proc set_monospace_width_proc \
	[namespace code get_mono_width_types]

#
# Tab completion possibilities for `set_monospace_width_proc`.
#
proc get_mono_width_types {args} {
	return {default east_asian jp}
}

set_help_text set_monospace_width_proc \
{Usage: set_monospace_width_proc ?default|jp|east_asian|<any procedure>?
Switch string-length getter. Currently, Fullwidth characters are considered
by pre-installed default, jp and east_asian.
Also currenly, jp and east_asian do same. It treats ●, ┤, α or Б 
characters as Fullwidth.
(This help is only available after once TAB pressed.)}

proc set_monospace_width_proc {proc} {
	variable monospace_width_proc
	switch $proc {
		east_asian - 
		jp { 
			set monospace_width_proc \
				[namespace code monospace_width_proc_east_asian]
		}
		default { ; # argument is "default"
			set monospace_width_proc \
				[namespace code monospace_width_proc_default]
		}
		default { ; # argument is others.
			set monospace_width_proc $proc
		}
	}
}

#
# Returns mono-spaced width of given string.
#
# Currently, only detect "definitely" fullwidth characters,
# indicated "F" or "W" in Unicode database.
# https://www.unicode.org/reports/tr11/
#
# Only BMP (Basic Multilingual Plane) is supported because
# Tcl 8 does not support other Planes.
#  
proc monospace_width_proc_default {s} {
	set f_and_w_re {[ᄀ-ᅟ⌚-⌛〈-〉⏩-⏬⏰⏳◽-◾☔-☕☰-☷♈-♓♿⚊-⚏⚓⚡⚪-⚫⚽-⚾⛄-⛅⛎⛔⛪⛲-⛳⛵⛺⛽✅✊-✋✨❌❎❓-❕❗➕-➗➰➿⬛-⬜⭐⭕⺀-⺙⺛-⻳⼀-⿕⿰-〾ぁ-ゖ゙-ヿㄅ-ㄯㄱ-ㆎ㆐-㇥㇯-㈞㈠-㉇㉐-ꒌ꒐-꓆ꥠ-ꥼ가-힣豈-﫿︐-︙︰-﹒﹔-﹦﹨-﹫！-｠￠-￦]}
	set c [regexp -all $f_and_w_re $s]
	return [expr {[string length $s] + $c}]
}

#
# Addition to default, "A" characters are valued as fullwidth.
#
proc monospace_width_proc_east_asian {s} {
	set f_and_w_re {[¡¤§-¨ª­-®°-´¶-º¼-¿ÆÐ×-ØÞ-áæè-êì-íðò-ó÷-úüþāđēěĦ-ħīı-ĳĸĿ-łńň-ŋōŒ-œŦ-ŧūǎǐǒǔǖǘǚǜɑɡ˄ˇˉ-ˋˍː˘-˛˝˟̀-ͯΑ-ΡΣ-Ωα-ρσ-ωЁА-яёᄀ-ᅟ‐–-‖‘-’“-”†-•․-‧‰′-″‵※‾⁴ⁿ₁-₄€℃℅℉ℓ№℡-™ΩÅ⅓-⅔⅛-⅞Ⅰ-Ⅻⅰ-ⅹ↉←-↙↸-↹⇒⇔⇧∀∀∂-∃∇-∈∋∏∑∕√∝-∠∣∥∧-∬∮∴-∷∼-∽≈≌≒≠-≡≤-≧≪-≫≮-≯⊂-⊃⊆-⊇⊕⊙⊥⊿⌒⌚-⌛〈-〉⏩-⏬⏰⏳①-ⓩ⓫-╋═-╳▀-▏▒-▕■-□▣-▩▲-△▶-▷▼-▽◀-◁◆-◈○◎-◑◢-◥◯◽-◾★-☆☉☎-☏☔-☕☜☞☰-☷♀♂♈-♓♠-♡♣-♥♧-♪♬-♭♯♿⚊-⚏⚓⚞-⚟⚡⚪-⚫⚽-⚿⛄-⛡⛣⛨-⛿✅✊-✋✨✽❌❎❓-❕❗❶-❿➕-➗➰➿⬛-⬜⭐⭕-⭙⺀-⺙⺛-⻳⼀-⿕⿰-〾ぁ-ゖ゙-ヿㄅ-ㄯㄱ-ㆎ㆐-㇥㇯-㈞㈠-ꒌ꒐-꓆ꥠ-ꥼ가-힣-﫿︀-︙︰-﹒﹔-﹦﹨-﹫！-｠￠-￦�]}
	set c [regexp -all $f_and_w_re $s]
	return [expr {[string length $s] + $c}]
}

namespace export file_tab_completion
namespace export format_to_columns_from_cpp
namespace export generic_tab_completion
namespace export get_mono_width_types
namespace export set_monospace_width_proc
namespace export set_tab_alternation_trigger
}

namespace import tab_completion::*
