# these procs are generic and are supposed to be moved to a generic OSD
# library or something similar.

proc osd_box {name args} {

#set parents widget
osd create rectangle $name -alpha 0

#init default properties
set background 0xffffffff
set border 1

#set properties for the parent widget
#
#how to handle?
#	-alpha
#	-rgb
foreach {var val} $args {

	switch -- $var {
		-rgba
			{set background $val}
		-border
			{set border $val} 
		-fill
			{osd configure $name -rgba $val}
		default
			{osd configure $name $var $val
			 #puts "The value of $var is $val"}
	}
}

#Set child widget properties
	osd create rectangle $name.top -relx 0.0 -rely 0.0 -relw 1.0 -h $border -rgba $background
	osd create rectangle $name.bottom -relx 0.0 -rely 1.0 -relw 1.0 -h -$border -rgba $background
	osd create rectangle $name.left -relx 0.0 -rely 0.0 -relh 1.0 -w $border -rgba $background
	osd create rectangle $name.right -relx 1.0 -rely -0.0 -relh 1.0 -w -$border -rgba $background
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
