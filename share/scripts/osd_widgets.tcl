# these procs are generic and are supposed to be moved to a generic OSD
# library or something similar.

proc osd_box {name args} {
	# process arguments
	set b 1 ;# border width
	set child_props  [list]
	set parent_props [list]
	foreach {key val} $args {
		switch -- $key {
		-rgba -
		-rgb -
		-alpha
			{lappend child_props $key $val}
		-border
			{set b $val}
		-fill
			{lappend parent_props -rgba $val}
		default
			{lappend parent_props $key $val}
		}
	}

	# parent widget
	eval "osd create rectangle \{$name\} -alpha 0 $parent_props"

	# child widgets, take care to not draw corner pixels twice (matters
	# for semi transparent colors), also works for negative border widths
	set bn [expr -$b]
	eval "osd create rectangle \{$name.top\} \
		-relx 0 -rely 0 -relw  1 -w $bn -h $b  $child_props"
	eval "osd create rectangle \{$name.right\} \
		-relx 1 -rely 0 -relh  1 -w $bn -h $bn $child_props"
	eval "osd create rectangle \{$name.bottom\} \
		-relx 1 -rely 1 -relw -1 -w $b  -h $bn $child_props"
	eval "osd create rectangle \{$name.left\} \
		-relx 0 -rely 1 -relh -1 -w $b  -h $b  $child_props"

	return $name
}

proc create_power_bar {name w h barcolor background edgecolor} {
	osd create rectangle $name        -rely 999 -relw $w -relh $h -rgba $background
	osd create rectangle $name.top    -x -1 -y   -1 -relw 1 -w 2 -h 1 -rgba $edgecolor
	osd create rectangle $name.bottom -x -1 -rely 1 -relw 1 -w 2 -h 1 -rgba $edgecolor
	osd create rectangle $name.left   -x   -1 -y -1 -w 1 -relh 1 -h 2 -rgba $edgecolor
	osd create rectangle $name.right  -relx 1 -y -1 -w 1 -relh 1 -h 2 -rgba $edgecolor
	osd create rectangle $name.bar    -relw 1 -relh 1 -rgba $barcolor -z 18
	osd create text      $name.text   -x 0 -y -6 -size 4 -rgba $edgecolor
}

proc update_power_bar {name x y power text} {
	if {$power > 1.0} {set power 1.0}
	if {$power < 0.0} {set power 0.0}
	osd configure $name -relx $x -rely $y
	osd configure $name.bar -relw $power
	osd configure $name.text -text "$text"
}

proc hide_power_bar {name} {
	osd configure $name -rely 999
}
