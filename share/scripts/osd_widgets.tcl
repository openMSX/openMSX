namespace eval osd_widgets {

set_help_text osd_box\
{The command 'osd_box' takes in the same parameters as an 'osd create' command. There are a few exceptions:

-fill: Defines the color within the box with a certain color and or alpha.
    Example: '-fill 0xff000080' will fill the box with a red color which 
    is 50% transparent
-border: Defines the border-width
    Example '-border 3' will create a box with a border 3 pixels wide

The following values are redirected to the osd box border:
    -rgba
    -rgb
    -alpha}

set_help_text create_power_bar\
{The command 'create_power_bar' takes in the following parameters:
    -name == Name of the power bar
    -w == Width of the power bar (in pixels)
    -h == Height of the power bar
    -barcolor == Powerbar color 
    -background == When power declines this color is shown
    -edgecolor == This is the edge color (try white when I doubt which color to use)

colors must have the following format 0xRRGGBBAA

The power bar is initially  created outside the viewable area we need to 
invoke the 'updated_power_bar' command to make it visible}

set_help_text update_power_bar\
{The command 'update_power_bar' takes in the following parameters:
    -name == Name of the power bar
    -x == vertical position of the power bar
    -y == horizontal position of the power bar
    -power == fill rate of the power bar in decimal percentages (10% == 0.1)
    -text == text to be printed above the power bar}

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

namespace export *

};# namespace osd_widgets

namespace import osd_widgets::*