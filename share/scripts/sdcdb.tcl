# SDCDB Debugger in Tcl - Version 0.7
#
# Copyright 2025 Pedro de Medeiros all rights reserved
#
# A debugger written in Tcl that uses `SDCC --debug` parameter to create breakpoints and much more.
#
# The main difference between SDCC Debugger's breakpoints and the built-in OpenMSX breakpoints is the
# C code integration:
# [*] allow users to create breakpoints on C code:
#     > sdcdb break main.c:55
# [*] list C source code at line 100:
#     > sdcdb list main.c:100
# [*] and more things to come (WIP).
#
# All commands that the SDCDB Debugger recognizes directly. You may call them with sdcdb [COMMAND] [ARGS...]:
#
# open path_to_CDB_file
#       - Read CDB file specified by a path parameter
# add ?-recursive? path_to_source_dir
#       - Scan directory for C source files
# break line
#       - creates breakpoint in <file>:<line>
# list ?line?
#       - list source code at <file>:<line>[-<line>]
# info ?-break?
#       - Display information on source code under the program counter. The -break
#         parameter stops execution.
# step ?n?
#       - Executes next n lines of C code step by step, proceeding through subroutine calls.
#         n defaults to 1.
# quit
#       - Free all used memory.
#
# Known limitations:
# [*] don't create C files with same name in different folders (in projects with multiple
#     folders), since CDB files don't keep track of directories, just file names. SDCDB
#     will get lost and report the wrong position because of missing files during break or
#     step.
# [*] don't create projects with assembly and C files with the same name, since SDCC
#     already creates an .asm file for every .c file in your project. SDCDB will get lost
#     and report the wrong position because of missing files during break or step.
#
# For more information about the CDB file format: https://sourceforge.net/p/sdcc/wiki/Home/

namespace eval sdcdb {

set ::env(DEBUG) 1
variable initialized    0
variable c_files_count                  ;# C files reference
variable a_files_count                  ;# ASM files reference
array set c_files       {}              ;# pool of c source files
array set a_files       {}              ;# pool of a source files
array set addr2file     {}              ;# array that maps to source from address
array set g_func2addr   {}              ;# array that maps to address from source (global)
# array set c_<filename>2addr           ;# same, but dinamically created array for each SDCC module (C file)
# array set a_<filename>2addr           ;# same, but dinamically created array for each SDCC module (ASM file)
# array set <filename>_func2addr        ;# array that maps function to source (global)
variable current_cond   {}              ;# debugger step/next boundary: {begin end} addresses
variable current_file   {}              ;# debugger file
variable current_line   {}              ;# debugger line
variable cond           {}              ;# breakpoint check condition
variable old_PC         {}
variable times_left     0
proc ASM {} { return ".asm" }

set_help_proc sdcdb [namespace code sdcdb_help]
proc sdcdb_help {args} {
    if {[llength $args] == 1} {
        return {The SDCDB debugger in Tcl connects OpenMSX to the CDB file created by SDCC.

Recognized commands: open, add, break, list, step, info, whereis, quit

Type 'help sdcdb <command>' for more information about each command.
}
    }
    switch -- [lindex $args 1] {
        "open" { return {Opens CDB file and start debugging session

Syntax: sdcdb open <pathToCDBFile>
}}
        "add" { return {Adds directory to be scanned for source files

'dir' is the path to a directory that will be scanned for more source files to be added to the source database. -recursive may be specified to also look for files in subdirectories.

Syntax: sdcdb add ?-recursive? <dir>
}}
        "break" { return {Creates a breakpoint

Create a OpenMSX breakpoint, but using the C source files as reference. 'sdcdb info -break' replaces 'debug break' for extra details about C code execution.

Syntax: sdcdb break <file>:<line>
        sdcdb break <file>:<functionName>
        sdcdb break <functionName>
}}
        "list" { return {Lists contents of a C source file

Without parameters, 'sdcdb list' returns the C source code under the PC register.

Syntax: sdcdb list
        sdcdb list <file>:<line>
        sdcdb list <file>:<functionName>
        sdcdb list <functionName>
}}
        "step" { return {Steps through next n lines of C code

The 'sdcdb step' command will proceed through subroutine calls. You may specify how many times 'sdcdb step' should execute (defaults to 1).

Syntax: sdcdb step ?n?
}}
        "info" { return {Displays information about current line of source code

A '-break' parameter stops execution after displaying the information.

Syntax: sdcdb info ?-break?
}}
        "whereis" { return {Displays if source file was found in the database

Syntax: sdcdb whereis <sourceFileName>
}}
        "quit" { return {Closes files and frees all memory.

Syntax: sdcdb quit}}
    }
}

proc output {args} {
    set chan stdout
    puts $chan [join $args " "]
    flush $chan
}

proc warn {args} {
    if {[env DEBUG]} {
        set chan stderr
        set msg [string map {"\n" "\\n"} [join $args " "]]
        puts $chan $msg
        flush $chan
    }
}

proc env {varname {defaults {}}} {
    if {[info exists ::env($varname)]} {
        return $::env($varname);
    }
    return $defaults;
}

proc sdcdb {args} {
    if {[env DEBUG]} {
        if {[catch {set result [dispatcher {*}$args]} fid]} {
            puts stderr $::errorInfo
            error $::errorInfo
        }
    } else {
        set result [dispatcher {*}$args]
    }
    return $result
}

proc dispatcher {args} {
    set params "[lrange $args 1 end]"
    set cmd [lindex $args 0]
    switch -- $cmd {
        "open"  { return [sdcdb_open       {*}$params] }
        add     { return [sdcdb_add        {*}$params] }
        whereis { return [sdcdb_whereis    {*}$params] }
        quit    { return [sdcdb_quit       {*}$params] }
    }
    # remaining commands need initialization
    variable initialized
    if {!$initialized} {
        error "SDCDB not initialized"
    }
    switch -- $cmd {
        "break" { return [sdcdb_break      {*}$params] }
        "list"  { return [sdcdb_list       {*}$params] }
        "info"  { return [sdcdb_info       {*}$params] }
        step    { return [sdcdb_step       {*}$params] }
        "next"  { return [sdcdb_next       {*}$params] }
        default { error "Unknown command \"[lindex $args 0]\"." }
    }
}

proc sdcdb_open {path} {
    variable initialized
    if {$initialized} {
        error "SDCDB already initialized"
    }
    set initialized 1
    set file [glob -type f -directory $path *.cdb]
    if {[llength $file] ne 1} {
        error "unique regular CDB file not found"
    }
    sdcdb_add $path
    read_cdb $file
    process_data
}

proc complete {arrayname name list} {
    variable $arrayname
    if {![info exists ${arrayname}($name)]} { warn "$arrayname\($name\) ignored"; return 0 }
    set ${arrayname}($name) [concat {*}[set ${arrayname}($name)] {*}$list]
    return 1
}

proc read_cdb {fname} {
    variable c_files_count
    variable addr2file
    set fh [open $fname "r"]
    # function pattern: search for "F:G$function_name$..." lines
    # 'F:G$debug_break$0_0$0({2}DF,SV:S),Z,0,0,0,0,0'
    set func_pat {^F:(G|F([^$]+)|L([^$]+))\$([^$]+)\$.*}
    # C line number pattern: search for "L:C$filename$line$level$block:address" lines
    set cline_pat {^L:C\$([^$]+)\$([^$]+)\$([^$]+)\$([^$]+):(\S+)$}
    # ASM line number pattern: search for "L:A$filename$line:address" lines
    set aline_pat {^L:A\$([^$]+)\$([^$]+):([^$]+)$}
    # function begin pattern: search for "L:(G|F<name>|L<name>)$function$level$block:address" lines
    set func_bn_pat {^L:(G|F([^$]+)|L([^$]+))\$([^$]+)\$([^$]+)\$([^$]+):(\S+)$}
    # function end pattern: search for "L:X(G|F<name>|L<name>)$function$level$block:address" lines
    set func_ed_pat {^L:X(G|F([^$]+)|L([^$]+))\$([^$]+)\$([^$]+)\$([^$]+):(\S+)$}
    set c_count 0  ;# lines of C code
    set a_count 0  ;# lines of asm code
    set gf_count 0 ;# global functions
    set sf_count 0 ;# static functions
    while {[gets $fh line] != -1} {
        set match [regexp -inline $func_pat $line]
        if {[llength $match] == 5} {
            lassign $match {} context {} {} funcname
            switch -- [string index $context 0] {
                G {
                    variable g_func2addr
                    set g_func2addr($funcname) {}
                    warn "g_func2addr\($funcname\) created"
                }
                F {
                    set arrayname [string range $context 1 [string length $context]]_func2addr
                    variable $arrayname
                    set ${arrayname}($funcname) {}
                    warn "${arrayname}\($funcname\) created"
                }
                L {
                    ;#error "not implemented yet '$line' \[1\]"
                }
            }
            continue
        }
        set match [regexp -inline $cline_pat $line]
        if {[llength $match] == 6} {
            lassign $match {} filename linenum {} {} address
            incr c_files_count($filename)
            # Put line -> address mapping of array with dynamic name
            set arrayname c_[file rootname $filename]2addr
            if {$c_files_count($filename) eq 1} {
                warn "Created new dynamic array $arrayname"
            }
            variable $arrayname
            set ${arrayname}($linenum) [expr 0x$address]
            set addr2file([expr 0x$address]) [list $filename $linenum]
            warn "mapping C source ${arrayname}\($linenum\): [set ${arrayname}($linenum)]"
            incr c_count
            continue
        }
        set match [regexp -inline $aline_pat $line]
        if {[llength $match] == 4} {
            lassign $match {} filename linenum address
            incr a_files_count($filename[ASM])  ;# SDCC removes the file extension for some reason
            # Put line -> address mapping of array with dynamic name
            set arrayname a_[file rootname $filename]2addr
            if {$a_files_count($filename[ASM]) eq 1} {
                warn "Created new dynamic array $arrayname"
            }
            variable $arrayname
            set ${arrayname}($linenum) [expr 0x$address]
            set addr2file([expr 0x$address]) [list $filename[ASM] $linenum]
            warn "mapping asm source ${arrayname}\($linenum\): [set ${arrayname}($linenum)]"
            incr a_count
            continue
        }
        set match [regexp -inline $func_bn_pat $line]
        if {[llength $match] == 8} {
            lassign $match {} context {} {} funcname {} {} address
            # Put function begin record in array with dynamic name
            switch -- [string index $context 0] {
                G {
                    variable g_func2addr
                    if {[complete g_func2addr $funcname [list [expr 0x$address]]]} {
                        warn "1:mapping function g_func2addr\($funcname\): [set g_func2addr($funcname)]"
                        incr gf_count
                    }
                }
                F {
                    set arrayname [string range $context 1 [string length $context]]_func2addr
                    variable $arrayname
                    if {[complete $arrayname $funcname [list [expr 0x$address]]]} {
                        warn "1:mapping function ${arrayname}\($funcname\): [set ${arrayname}($funcname)]"
                        incr sf_count
                    }
                }
                L {
                    ;#error "not implemented yet '$line' \[2\]"
                }
            }
            continue
        }
        set match [regexp -inline $func_ed_pat $line]
        if {[llength $match] == 8} {
            lassign $match {} context {} {} funcname {} {} address
            # Put function end record in array with dynamic name
            switch -- [string index $context 0] {
                G {
                    variable g_func2addr
                    if {[complete g_func2addr $funcname [list [expr 0x$address]]]} {
                        warn "2:mapping function g_func2addr\($funcname\): [set g_func2addr($funcname)]"
                    }
                }
                F {
                    set arrayname [string range $context 1 [string length $context]]_func2addr
                    variable $arrayname
                    if {[complete $arrayname $funcname [list [expr 0x$address]]]} {
                        warn "2: mapping function ${arrayname}\($funcname\): [set ${arrayname}($funcname)]"
                    }
                }
                L {
                    ;#error "not implemented yet '$line' \[3\]"
                }
            }
            continue
        }
        ;#warn "Ignored '$line'"
    }
    close $fh
    output "[array size c_files_count] C files references added"
    output "[array size a_files_count] assembly files references added"
    output "$c_count C source lines found"
    output "$a_count assembly source lines found"
    output "$gf_count global function(s) registered"
    output "$sf_count static function(s) registered"
}

proc print_var {arrayname} {
    variable $arrayname
    foreach key [array names $arrayname] {
        output "* $key: ${arrayname}($key)"
    }
}

proc fix_blank_spaces {arrayname} {
    variable $arrayname 
    set count 0
    set old_key {}
    foreach key [lsort -integer [array names $arrayname]] {
        if {$old_key ne {} && [expr $key - 1] != $old_key} {
            for {set i [expr $old_key + 1]} {$i < $key} {incr i} {
                #warn "Setting $arrayname\($i\) to ${arrayname}\($old_key\) = [set ${arrayname}($old_key)]"
                set ${arrayname}($i) [set ${arrayname}($old_key)]
                incr count
            }
        }
        set old_key $key
    }
    return $count
}

proc fix_last_address {arrayname} {
    variable $arrayname
    set old_key {}
    set sorted [lsort -integer [array names $arrayname]]
    foreach key $sorted {
        if {$old_key ne {} && [set ${arrayname}($old_key)] ne [set ${arrayname}($key)]} {
            set ${arrayname}($old_key) [list [set ${arrayname}($old_key)] [expr [set ${arrayname}($key)] - 1]]
            incr count
        } elseif {$old_key ne {}} {
            set ${arrayname}($old_key) [list [set ${arrayname}($old_key)] [set ${arrayname}($old_key)]]
        }
        set old_key $key
    }
    if {[llength $sorted] > 0} {
        # change last value
        set last [lindex $sorted end]
        set ${arrayname}($last) [list [set ${arrayname}($last)] [set ${arrayname}($last)]]
        incr count
    }
}

proc process_data {} {
    variable c_files
    foreach filename [array names c_files] {
        set arrayname c_[file rootname $filename]2addr
        variable $arrayname
        warn "0fix_last_address: $arrayname"
        fix_last_address $arrayname
        fix_blank_spaces $arrayname
    }
    variable a_files
    foreach filename [array names a_files] {
        set arrayname a_[file rootname $filename]2addr
        variable $arrayname
        warn "1fix_last_address: $arrayname"
        fix_last_address $arrayname
        fix_blank_spaces $arrayname
    }
    fix_blank_spaces addr2file
    return
}

proc normal_glob {dir pattern} {
    return [glob -nocomplain -type f -directory $dir $pattern]
}

proc recursive_glob {dir pattern} {
    set result {}
    foreach file [glob -nocomplain -directory $dir $pattern] {
        lappend result $file
    }
    foreach subdir [glob -nocomplain -directory $dir *] {
        if {[file isdirectory $subdir]} {
            set sub_results [recursive_glob $subdir $pattern]
            set result [concat $result $sub_results]
        }
    }
    return $result
}

proc add_files_to_database {arrayname files} {
    variable $arrayname
    foreach path $files {
        set filename [file tail $path]
        if {[array get $arrayname $filename] ne {}} {
            warn "file '$filename' already registered in $arrayname, new entry ignored."
        }
        set ${arrayname}($filename) $path
    }
}

proc sdcdb_add {param {path {}}} {
    if {$param eq "-recursive"} {
        set function recursive_glob
    } else {
        set path $param
        set function normal_glob
    }
    set new_files [$function $path *.c]
    warn "adding files: [join $new_files {, }]"
    add_files_to_database c_files $new_files
    set new_files [$function $path *[ASM]]
    warn "adding files: [join $new_files {, }]"
    add_files_to_database a_files $new_files
}

proc sdcdb_list {args} {
    set pattern1 {([^:]+):(\d+)(-(\d+))?}
    set pattern2 {([^:]+):(\S+)}
    set pattern3 {(\S+)}
    set arg [lindex $args 0]
    set match [regexp -inline $pattern1 $arg]
    if {[llength $match] == 5} {
        lassign $match {} filename start {} end
        return [list_file $filename $start $end]
    }
    set match [regexp -inline $pattern2 $arg]
    if {[llength $match] == 3} {
        lassign $match {} filename funcname
        return [list_fun $filename $funcname]
    }
    set match [regexp -inline $pattern3 $arg]
    if {[llength $match] == 2} {
        lassign $match {} funcname
        return [list_fun {} $funcname]
    }
    return [list_pc]
}

proc list_file {file start {end {}} {focus -1} {showerror 0}} {
    if {[file extension $file] eq ".c"} {
        variable c_files
        set record [array get c_files $file]
        if {$record eq {}} {
            if {$showerror} {
                error "file '$file' not found in database, add a directory that contains such file with 'sdcdb add <dir>'"
            } else {
                output "$file: not found"
            }
            return
        }
    }
    if {[file extension $file] eq [ASM]} {
        variable a_files
        set record [array get a_files $file]
        if {$record eq {}} {
            if {$showerror} {
                error "file '$file' not found in database, add a directory that contains such file with 'sdcdb add <dir>'"
            } else {
                output "$file: not found"
            }
            return
        }
    }
    set fh [open [lindex $record 1] r]
    set pos 0
    # 10 lines by default
    if {$end eq {}} { set end [expr $start + 9] }
    while {[gets $fh line] >= 0} {
        incr pos
        if {$pos >= $start} {
            if {$pos == $focus} {
                output "[format %-5d $pos]*    $line"
            } else {
                output "[format %-5d $pos]:    $line"
            }
        }
        if {$pos >= $end} { break }
    }
    close $fh
}

proc list_pc {{x0 -1} {x1 9}} {
    variable addr2file
    if {[array get addr2file [reg PC]] ne {}} {
        debug break  ;# makes no sense to list a running program
        lassign $addr2file([reg PC]) file begin
        return [list_file $file [expr $begin + $x0] [expr $begin + $x1] $begin]
    }
    output "address database not found for 0x[format %04X [reg PC]]"
}

proc list_fun {filename funcname} {
    lassign [scanfun $filename $funcname] begin end
    variable addr2file
    if {[array get addr2file $begin] ne {}} {
        lassign $addr2file($begin) file begin
        lassign $addr2file($end)   {}   end
        if {$file ne $filename} {
            error "function database '$funcname' not found in '$filename'"
        }
        return [list_file $file [expr $begin - 1] $end]
    }
    error "function database '$funcname' not found"
}

# scan file database then global database for function name
proc scanfun {filename funcname} {
    if {$filename ne {}} {
        set arrayname [file rootname $filename]_func2addr
        variable $arrayname
        if {![info exists $arrayname]} {
            error "file database '$arrayname' not found"
        }
        set record [lindex [array get $arrayname $funcname] 1]
    } else {
        set record {}
    }
    if {$record eq {}} {
        # not found in file, search globally
        variable g_func2addr
        set record [lindex [array get g_func2addr $funcname] 1]
    }
    set result $record
    return [eval list $result]
}

# reverse function search (global first, then search all files)
proc scanfun_r {funcname} {
    variable g_func2addr
    set record [lindex [array get g_func2addr $funcname] 1]
    if {$record eq {}} {
        variable c_files
        foreach filename c_files {
            set arrayname [file rootname $filename]_func2addr
            variable $arrayname
            if {[info exists ${arrayname}($funcname)]} {
                set record [lindex [array get $arrayname $line] 1]
                break
            }
        }
    }
    set result $record
    return [eval list $result]
}

proc search_file_array {file line} {
    set tmp [file rootname $file]
    if {[file extension $file] eq ".c"} {
        set arrayname c_${tmp}2addr
        variable c_${tmp}2addr
    } else {
        set arrayname a_${tmp}2addr
        variable a_${tmp}2addr
    }
    if {![info exists $arrayname]} {
        error "'$file': source file not found ($arrayname)"
    }
    set result [array get $arrayname $line]
    if {$result eq {}} {
        error "line $line not found in file '$file'"
    }
    return [lindex $result 1]
}

proc sdcdb_break {pos {cond {}} {cmd {sdcdb info -break}}} {
    set pattern1 {([^:]+):(\d+)}
    set pattern2 {([^:]+):(\S+)}
    set pattern3 {(\S+)}
    set match [regexp -inline $pattern1 $pos]
    if {[llength $match] == 3} {
        lassign $match {} filename linenum
        set record [search_file_array $filename $linenum]
        if {$record ne {}} {
            debug breakpoint create -address [lindex $record 1] -condition $cond -command $cmd
            return
        } else {
            error "address not found"
        }
    }
    set match [regexp -inline $pattern2 $pos]
    if {[llength $match] == 3} {
        lassign $match {} filename funcname
        lassign [scanfun $filename $funcname] start {}
        if {$start ne {}} {
            debug breakpoint create -address $start -condition $cond -command $cmd
            return
        } else {
            error "function not found"
        }
    }
    set match [regexp -inline $pattern3 $pos]
    if {[llength $match] == 2} {
        lassign $match {} funcname
        lassign [scanfun {} $funcname] start {}
        if {$start eq {}} {
            error "function not found"
        }
        debug breakpoint create -address $start -condition $cond -command $cmd
        return
    } else {
        error "error parsing break command"
    }
}

proc check {} {
    variable old_PC
    return [reg PC] != $old_PC
}

proc update_result {} {
    # outside current C instruction?
    variable current_file
    variable current_line
    set record [find_source [reg PC]]
    # has position in file changed?
    if {$record ne {} && $record ne [list $current_file $current_line]} {
        variable times_left
        lassign $record current_file current_line
        output "file: $current_file:$current_line, position: 0x[format %04X [reg PC]] (loop $times_left)"
        list_pc -1 1
        #lassign $current_cond begin end
        incr times_left -1
        if {$times_left <= 0} {
            output "done."
            variable cond
            debug condition remove $cond
            set cond {}
            debug break
        } else {
            debug cont
        }
    }
    #if {[reg PC] < $begin || [reg PC] > $end}
}

proc find_source {address} {
    variable addr2file
    # get current source file
    set record [lindex [array get addr2file [reg PC]] 1]
    if {$record ne {}} {
        warn "record: 0x[format %04X [reg PC]] -> $record"
        return $record
    }
    return {}
}

proc sdcdb_step {{n 1}} {
    debug break  ;# just to be sure
    variable cond
    if {$cond eq {}} {
        variable old_PC [reg PC]
        set cond [debug condition create -condition {[sdcdb::check]} -command sdcdb::update_result]
    }
    # get current position in source code
    set record [find_source [reg PC]]
    lassign $record file line
    lassign [search_file_array $file $line] begin end
    warn "search_file_array: $file, $line -> [search_file_array $file $line]"
    if {$begin ne {} && $end ne {}} {
        variable current_file $file
        variable current_line $line
        variable current_cond [list $begin $end]
        variable times_left $n
        debug cont
    } else {
        output "out of scope, skipping till we come back."
        debug cont
    }
}

proc sdcdb_next {{n 1}} {
    sdcdb_step $n
}

proc print_info {} {
    variable addr2file
    if {[array get addr2file [reg PC]] ne {}} {
        lassign $addr2file([reg PC]) file line
        if {$file eq {}} {
            error "address [format %04X [reg PC]] not found in database."
        }
        list_file $file [expr $line - 1] [expr $line + 1] $line
    } else {
        error "line mapping not found for 0x[format %04X [reg PC]]"
    }
}

proc sdcdb_info {arg} {
    if {$arg eq "-break"} {
        debug break
    }
    if {[catch {print_info} fid]} {
        output $fid
    }
}

proc sdcdb_whereis {file} {
    variable c_files
    if {[info exists c_files($file)]} {
        output "'$file' found in database (as '[set c_files($file)]')"
        return
    }
    variable a_files
    if {[info exists a_files($file)]} {
        output "'$file' found in database (as '[set a_files($file)]')"
        return
    }
    output "'$file' not found in database"
}

proc sdcdb_quit {} {
    variable initialized
    if {$initialized} {
        variable c_files
        foreach filename c_files {
            set arrayname c_[file rootname $filename]2addr
            variable $arrayname
            unset $arrayname
            set arrayname [file rootname $filename]_func2addr
            variable $arrayname
            unset $arrayname
        }
        foreach filename a_files {
            set arrayname a_[file rootname $filename]2addr
            variable $arrayname
            unset $arrayname
        }
        unset c_files
        variable c_files_count
        unset c_files_count
        variable addr2file
        unset addr2file
        variable g_func2addr
        unset g_func2addr
        output "done."
    }
}

namespace export sdcdb

}

# Import sdcdb exported functions
namespace import sdcdb::*
