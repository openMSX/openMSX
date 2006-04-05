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
proc monitor_monochrome {} {
	set ::color_matrix { { .129 .252 .049 } { .257 .504 .098 } { .129 .252 .049 } }
}

