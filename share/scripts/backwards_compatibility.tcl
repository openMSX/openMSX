# Provide 'quit' command for backwards compatibility. The prefered command is
# now 'exit'. TCL normally only has a 'exit' command. Also most shells have
# exit but no quit.
proc quit { } {
	exit
}
