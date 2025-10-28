
namespace eval file_completion {
#
# BE CAREFUL TO RENAME OR/AND REPLACE! See 'src/command/Completer.cc'
# Tab completion procedure for file path.
# Directly used from C++ code and indirectly called from Tcl codes.
#
proc filetabcompletion {console_width target_paths extras args} {
	puts "--- [lindex $args end]"
	if {![llength $target_paths]} { lappend target_paths ./ }
	set target_full_paths ""
	foreach p $target_paths {
		# [file join] expands "." to current working directory,
		# and drop previous path when a following one is absolute path.
		set fp [weak_normalize_path . $p]
		if {[string index $fp end] ne "/"} { append fp / }
		lappend target_full_paths $fp
	}
	set child_dirs ""
	set child_files ""
	# $args should have two or more elements here.
	# (When only one or zero, this procedure is not called.)
	set our_arg [lindex $args end]
	# Check number mode.
	set number_order -2
	if {[regexp -indices "^(,\\d*)$|.*/(,\\d*)$" $our_arg m0 m1 m2]} {
		set midx [lindex $m1 0]
		if {$midx == -1} { set midx [lindex $m2 0] }
		set number_order [string range $our_arg [expr {$midx + 1}] end]
		if {$number_order eq ""} { set number_order -1 }
		set our_arg [string range $our_arg 0 [expr {$midx - 1}]]
	}
	# make clear to point directory by "/" for some dot paths.
	if {$our_arg eq "" || $our_arg eq "." } { set our_arg ./ }
	if {$our_arg eq ".." || [regexp /\\.\\.\$ $our_arg]} { append our_arg / }

	if {[file pathtype $our_arg] ne "relative"} {
		lassign [gather_directory_contents $our_arg] \
			dirstr tail child_dirs child_files
	} else {
		foreach p $target_full_paths {
			set full_path [weak_normalize_path $p $our_arg]
			# Note: because $our_arg is not empty, $full_path never be empty.
			if {[string index $our_arg end] eq "/" && \
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
	set child_dirs [lsort -dictionary -unique $child_dirs]
	set child_files [lsort -dictionary -unique $child_files]
	set entries [list {*}$child_dirs {*}$child_files {*}$extras]
	if {$number_order != -2} {
		return [update_in_number_mode_file_completion \
			$number_order $console_width $our_arg \
			$entries $dirstr $tail $target_full_paths]
	}
	return [filter_and_update_in_file_competion \
		$console_width $our_arg $entries $dirstr $tail $target_full_paths]
}

proc filter_and_update_in_file_competion { \
	console_width our_arg entries dirstr tail possible_parents \
} {
	set result $entries
	if {[string length $tail]} {
		set result ""
		foreach e $entries {
			if {![string first $tail $e]} { lappend result $e }
		}
	}
	if {![llength $result]} {
		puts "No file matches: $our_arg"
		return [list ---rewrite $our_arg]
	} elseif {1 == [llength $result]} {
		set result [lindex $result 0] ; # {any text} to "any text". Required.
		return [make_rewrite_or_done $dirstr$result $possible_parents]
	}
	show_possibilities result $console_width 
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
	return [list ---rewrite $our_arg]
}

proc update_in_number_mode_file_completion { \
	order_number console_width our_arg entries \
	dirstr tail possible_parents \
} {
	set result $entries
	if {![llength $result]} {
		puts "No file matches: $our_arg"
		return [list ---rewrite $our_arg]
	} elseif {1 == [llength $result]} {
		set result [lindex $result 0] ; # {any text} to "any text". Required.
		return [make_rewrite_or_done $dirstr$result $possible_parents]
	}
	if {0 <= $order_number && $order_number < [llength $entries]} {
		set result [lindex $entries $order_number]
		return [make_rewrite_or_done $dirstr$result $possible_parents]
	}
	set result ""
	set cnt 0
	foreach e $entries {
		lappend result ",${cnt}:$e"
		incr cnt
	}
	puts "Please type comma and number as the last component."
	puts "ex1) ,1   ex2) some/path/,4   ex3) /from/root/,7"
	show_possibilities result $console_width
	return [list ---rewrite $our_arg,]
}

proc make_rewrite_or_done {path {possible_parents {}} } {
	set is_dir [file isdirectory $path]
	foreach p $possible_parents {
		if {$is_dir} { break }
		if {[file isdirectory [file join $p $path]]} {
			set is_dir true
		}
	}
	return [list [expr {$is_dir ? "---rewrite": "---done"}] $path]
}

proc gather_directory_contents {path} {
	lassign [split_dir_and_tail $path false] dirstr tail
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
# Used when you don't want to resolve symbolic link but do
# [file normalize [file join ...]] .
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

proc show_possibilities {result_name console_width} {
	upvar $result_name result
	foreach l [format_to_columns result $console_width] {
		puts $l
	}
}

#
# Same purpose procedure of C++ 'openmsx::format_helper' in
# 'src/commands/Completer.cc'.
#
proc format_helper {lines max_column stops_name lengths_name} {
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

#
# Clone procedure of C++ 'openmsx::format' in 'src/commands/Completer.cc'.
#
proc format_to_columns {input_name max_column} {
	upvar $input_name input
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
		if {[format_helper $lines $max_column try_stops i_lengths]} {
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
			lset result $ln_i "[lindex $result $ln_i]$ws[lindex $input $ii]"
		}
		set col_l $col
	}
	return $result
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

proc get_mono_width_types {args} {
	return {default east_asian jp}
}

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

namespace export filetabcompletion
namespace export set_monospace_width_proc
namespace export get_mono_width_types
}

namespace import file_completion::*
