# these procs are generic and are supposed to be moved to a generic OSD
# library or something similar.

proc osd_box {name args} {
#todo: a lot

array set properties $args

#set parents widget
osd create rectangle $name -alpha 0

set background 0xff0000ff
set border 1

#set properties for the parent widget
foreach {var val} [array get properties] {
    puts "The value of $var is $val"

	#Standard OSD properties
    if {$var=="-alpha"} {osd configure $name -alpha $val} ;#how to handle this?
    if {$var=="-fadePeriod"} {osd configure $name -fadePeriod $val}
    if {$var=="-fadeTarget"} {osd configure $name -fadeTarget $val}

    if {$var=="-rgb"} {osd configure $name -rgb $val}	;#how to handle this?
    if {$var=="-scale"} {osd configure $name -scale $val}
    if {$var=="-type"} {osd configure $name -type $val}

    if {$var=="-h"} {osd configure $name -h $val}
    if {$var=="-x"} {osd configure $name -x $val}
    if {$var=="-z"} {osd configure $name -z $val}
    if {$var=="-w"} {osd configure $name -w $val}
    if {$var=="-y"} {osd configure $name -y $val}
    if {$var=="-clip"} {osd configure $name -clip $val} ;#what does this do?

    if {$var=="-image"} {osd configure $name -image $val}
    if {$var=="-relh"} {osd configure $name -relh $val}
    if {$var=="-relx"} {osd configure $name -relx $val}
    if {$var=="-relw"} {osd configure $name -relw $val}
    if {$var=="-rely"} {osd configure $name -rely $val}
    if {$var=="-scaled"} {osd configure $name -scaled $val} ;#what does this do?
    
    #custom parameters
    if {$var=="-rgba"} {set background $val}
    if {$var=="-border"} {set border $val}   
    if {$var=="-background"} {osd configure $name -rgba $val}
    
#eval osd create rectangle $name $args -alpha 0
}

#Set child widget properties
	osd create rectangle $name.child1 -relx 0.0 -rely 0.0 -relw 1.0 -h $border -rgba $background
	osd create rectangle $name.child2 -relx 0.0 -rely 1.0 -relw 1.0 -h -$border -rgba $background
	osd create rectangle $name.child3 -relx 0.0 -rely 0.0 -relh 1.0 -w $border -rgba $background
	osd create rectangle $name.child4 -relx 1.0 -rely -0.0 -relh 1.0 -w -$border -rgba $background
}

proc create_power_bar {name w h barcolor background edgecolor} {
	osd create rectangle $name        -rely 999 -relw $w -relh $h -rgba $background
	osd create rectangle $name.top    -x -1 -y   -1 -relw 1 -w 2 -h 1 -rgba $edgecolor
	osd create rectangle $name.bottom -x -1 -rely 1 -relw 1 -w 2 -h 1 -rgba $edgecolor
	osd create rectangle $name.left   -x   -1 -y -1 -w 1 -relh 1 -h 2 -rgba $edgecolor
	osd create rectangle $name.right  -relx 1 -y -1 -w 1 -relh 1 -h 2 -rgba $edgecolor
	osd create rectangle $name.bar    -relw 1 -relh 1 -rgba $barcolor
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
