# Provide 'quit' command for backwards compatibility. The prefered command is
# now 'exit'. TCL normally only has a 'exit' command. Also most shells have
# exit but no quit.
proc quit { } {
	puts stderr "This command ('quit') has been deprecated (and may be removed in a future release), please use 'exit' instead!"
	exit
}

proc decr { var { num 1 } } {
	puts stderr "This command ('decr $var $num') has been deprecated (and may be removed in a future release), please use 'incr $var -$num' instead!"
	uplevel incr $var [expr -$num]
}
proc restoredefault { var } {
	puts stderr "This command ('restoredefault $var') has been deprecated (and may be removed in a future release), please use 'unset $var' instead!"
	uplevel unset $var
}
proc alias { cmd body } {
	puts stderr "This command ('alias $cmd $body') has been deprecated (and may be removed in a future release), please define a proc instead: proc $cmd {} $body"
	proc $cmd {} $body
}
