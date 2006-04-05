proc monitor_normal {} {
	set ::color_matrix { { 1 0 0 } { 0 1 0 } { 0 0 1 } }
}
proc monitor_red {} {
	set ::color_matrix { { 1 0 0 } { 0 0 0 } { 0 0 0 } }
}
proc monitor_green {} {
	set ::color_matrix { { 0 0 0 } { 0 1 0 } { 0 0 0 } }
}
proc monitor_blue {} {
	set ::color_matrix { { 0 0 0 } { 0 0 0 } { 0 0 1 } }
}
proc monitor_monochrome_amber {} {
	set ::color_matrix { { .257 .504 .098 } { .193 .378 .073 } { 0 0 0 } }
}
proc monitor_monochrome_amber_bright {} {
	set ::color_matrix { { .333 .333 .333 } { .249 .249 .249 } { 0 0 0 } }
}
proc monitor_monochrome_green {} {
	set ::color_matrix { { .129 .252 .049 } { .257 .504 .098 } { .129 .252 .049 } }
}
proc monitor_monochrome_green_bright {} {
	set ::color_matrix { { .167 .167 .167 } { .333 .333 .333 } { .167 .167 .167 } }
}
proc monitor_monochrome_white {} {
	set ::color_matrix { { .257 .504 .098 } { .257 .504 .098 } { .257 .504 .098 } }
}
proc monitor_monochrome_white_bright {} {
	set ::color_matrix { { .333 .333 .333 } { .333 .333 .333 } { .333 .333 .333 } }
}
