package provide cheatdfinder 0.5
# Cheat finder version 0.5
#
# Welcome to the openMSX findcheater. Please visit
# http://forum.openmsx.org/viewtopic.php?t=32 For a quick tutorial
#
# Credits:
#   Copyright 2005 Wouter Vermaelen all rights reserved
#   Copyright 2005 Patrick van Arkel all rights reserved
#
# Usage:
#   findcheat [-start] [-max n] [expression]
#      -start	  :  restart search, discard previously found addresses
#      -max n     :  show max n results
#      expression :  TODO
#
# Examples:
#   findcheat 42                 search for specific value
#   findcheat bigger             search for values that have become bigger
#   findcheat new == (2 * old)   search for values that have doubled
#   findcheat new == (old - 1)   search for values that have decreased by 1
#   findcheat                    repeat the previous found addresses
#   findcheat -start new < 10    restart and search for values less then 10
#   findcheat -max 40 smaller    search for smaller values, show max 40 results
#

#set maximum to display cheats
set findcheat_max  15

# Restart cheat finder. Should not be used directly by the user
proc findcheat_start {} {
  global findcheat_mem
  set mem [debug read_block memory 0 0x10000]
  binary scan $mem c* values
  set addr 0
  foreach val $values {
    set findcheat_mem($addr) $val
    incr addr
  }
}

# Helper function to do the actual search.
# Should not be used directly by the user.
# Returns a list of triplets (addr, old, new)
proc findcheat_helper { expression } {
  global findcheat_mem
  set result [list]
  foreach {addr old} [array get findcheat_mem] {
    set new [debug read memory $addr]
    if [expr $expression] {
      set findcheat_mem($addr) $new
      lappend result [list $addr $old $new]
    } else {
      unset findcheat_mem($addr)
    }
  }
  return $result
}

# main routine
proc findcheat { args } {
  # create findcheat_mem array
  global findcheat_mem
  if ![array exists findcheat_mem] findcheat_start
  
  # get the maximum results to display
  global findcheat_max
    
  # build translation array for convenience expressions
  global findcheat_translate
  if ![array exists findcheat_translate] {
    # TODO add more here
    set findcheat_translate()         "true"
    
    set findcheat_translate(bigger)   "new > old"
    set findcheat_translate(smaller)  "new < old"
    
    set findcheat_translate(more)     "new > old"
    set findcheat_translate(less)     "new < old"
    
    set findcheat_translate(notequal) "new != old"
    set findcheat_translate(equal)    "new == old"
    
    set findcheat_translate(loe)      "new <= old"
    set findcheat_translate(moe)      "new >= old"
  }

  # parse options
  while (1) {
    switch -- [lindex $args 0] {
      "-max" {
        set findcheat_max  [lindex $args 1]
        set args [lrange $args 2 end]
      }
      "-start" {
        findcheat_start
        set args [lrange $args 1 end]
      }
      "default" break
    }
  }
  
  # all remaining arguments form the expression  
  set expression [join $args]
    
  if [info exists findcheat_translate($expression)] {
    # translate a convenience expression into a real expression
    set expression $findcheat_translate($expression)
  } elseif [string is integer $expression] {
    # search for a specific value
    set expression "new == $expression"
  }

  # prefix 'old', 'new' and 'addr' with '$'
  set expression [string map {old $old new $new addr $addr} $expression] 
  
  # search memory 
  set result [findcheat_helper $expression]
 
  #display the result 
  set num [llength $result]
  set output ""
  if {$num == 0} {
    return "No results left"
  } elseif {$num <= $findcheat_max} {
    set sorted [lsort -integer -index 0 $result]
    foreach {addr old new} [join $sorted] {
      append output "[format 0x%04X $addr] : $old -> $new \n"
    }
    return $output
  } else {
    return "$num results found -> Maximum result to display set to $findcheat_max "
  }
}