package provide cheatfinder 0.5
# Cheat finder version 0.5
#
# Welcome to the openMSX cheatfinder. Please visit
# http://forum.openmsx.org/viewtopic.php?t=32 For a quick tutorial
#
# Credits:
#   Copyright 2005 Wouter Vermaelen all rights reserved
#   Copyright 2005 Patrick van Arkel all rights reserved
#
# Usage:
#   cheatfind [-start] [-max n] [expression]
#      -start	  :  restart search, discard previously found addresses
#      -max n     :  show max n results
#      expression :  TODO
#
# Examples:
#   cheatfind 42                 search for specific value
#   cheatfind bigger             search for values that have become bigger
#   cheatfind new == (2 * old)   search for values that have doubled
#   cheatfind new == (old - 1)   search for values that have decreased by 1
#   cheatfind                    repeat the previous found addresses
#   cheatfind -start new < 10    restart and search for values less then 10
#   cheatfind -max 40 smaller    search for smaller values, show max 40 results
#

#set maximum to display cheats
set cheatfind_max  15

# Restart cheat finder. Should not be used directly by the user
proc cheatfind_start {} {
  global cheatfind_mem
  set mem [debug read_block memory 0 0x10000]
  binary scan $mem c* values
  set addr 0
  foreach val $values {
    set cheatfind_mem($addr) $val
    incr addr
  }
}

# Helper function to do the actual search.
# Should not be used directly by the user.
# Returns a list of triplets (addr, old, new)
proc cheatfind_helper { expression } {
  global cheatfind_mem
  set result [list]
  foreach {addr old} [array get cheatfind_mem] {
    set new [debug read memory $addr]
    if [expr $expression] {
      set cheatfind_mem($addr) $new
      lappend result [list $addr $old $new]
    } else {
      unset cheatfind_mem($addr)
    }
  }
  return $result
}

# main routine
proc cheatfind { args } {
  # create cheatfind_mem array
  global cheatfind_mem
  if ![array exists cheatfind_mem] cheatfind_start
  
  # get the maximum results to display
  global cheatfind_max
    
  # build translation array for convenience expressions
  global cheatfind_translate
  if ![array exists cheatfind_translate] {
    # TODO add more here
    set cheatfind_translate()         "true"
    
    set cheatfind_translate(bigger)   "new > old"
    set cheatfind_translate(smaller)  "new < old"
    
    set cheatfind_translate(more)     "new > old"
    set cheatfind_translate(less)     "new < old"
    
    set cheatfind_translate(notequal) "new != old"
    set cheatfind_translate(equal)    "new == old"
    
    set cheatfind_translate(loe)      "new <= old"
    set cheatfind_translate(moe)      "new >= old"
  }

  # parse options
  while (1) {
    switch -- [lindex $args 0] {
      "-max" {
        set cheatfind_max  [lindex $args 1]
        set args [lrange $args 2 end]
      }
      "-start" {
        cheatfind_start
        set args [lrange $args 1 end]
      }
      "default" break
    }
  }
  
  # all remaining arguments form the expression  
  set expression [join $args]
    
  if [info exists cheatfind_translate($expression)] {
    # translate a convenience expression into a real expression
    set expression $cheatfind_translate($expression)
  } elseif [string is integer $expression] {
    # search for a specific value
    set expression "new == $expression"
  }

  # prefix 'old', 'new' and 'addr' with '$'
  set expression [string map {old $old new $new addr $addr} $expression] 
  
  # search memory 
  set result [cheatfind_helper $expression]
 
  #display the result 
  set num [llength $result]
  set output ""
  if {$num == 0} {
    return "No results left"
  } elseif {$num <= $cheatfind_max} {
    set sorted [lsort -integer -index 0 $result]
    foreach {addr old new} [join $sorted] {
      append output "[format 0x%04X $addr] : $old -> $new \n"
    }
    return $output
  } else {
    return "$num results found -> Maximum result to display set to $cheatfind_max "
  }
}