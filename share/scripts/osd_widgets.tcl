namespace eval osd_widgets {

set help_text \
{The command 'osd_widgets::msx_init' takes one parameter, this parameter will be used as
our base layer which will be scaled according to the MSX resultion adjusted for
'set adjust', 'scale factor' and 'horizontal_stretch'.

All these compensation factors ('set adjust', ...) can change over time. So it
is needed to 'regularly' (e.g. in a 'after frame' callback) re-adjust the msx
layer. This can be done with the 'osd_widgets::msx_update' proc.

Example: osd_widgets::msx_init baselayer
         osd create rectangle baselayer.box -x 10 -y 10 -h 16 -w 16 -rgb 0xffffff
         ...
         osd_widgets::msx_update baselayer

This will display a white 16x16 box at MSX location x,y == 10,10.}
set_help_text osd_widgets::msx_init   $help_text
set_help_text osd_widgets::msx_update $help_text

proc msx_init {name} {
	osd create rectangle $name -scaled true -alpha 0
	msx_update $name
}

proc msx_update {name} {
	# compensate for horizontal-stretch and set-adjust
	set hstretch [expr {$::renderer != "SDL"} ? $::horizontal_stretch : 320]
	set xsize   [expr 320.0 / $hstretch]
	set xoffset [expr ($hstretch - 256) / 2 * $xsize]
	set ysize 1
	set lines [expr ([vdpreg 9] & 128) ? 212 : 192]
	set yoffset [expr (240 - $lines) / 2 * $ysize]
	set adjreg [vdpreg 18]
	set hadj [expr (($adjreg & 15) ^ 7) - 7]
	set vadj [expr (($adjreg >> 4) ^ 7) - 7]
	set xoffset [expr $xoffset + $xsize * $hadj]
	set yoffset [expr $yoffset + $ysize * $vadj]
	osd configure $name -x $xoffset -y $yoffset -w $xsize -h $ysize
}


set_help_text osd_widgets::box\
{The command 'osd_widgets::box' supports the same parameters as an 'osd create' command.
There are a few exceptions:

-fill: Defines the color within the box with a certain color and or alpha.
    Example: '-fill 0xff000080' will fill the box with a red color which
    is 50% transparent
-border: Defines the border-width
    Example '-border 3' will create a box with a border 3 pixels wide

The following values are redirected to the osd box border:
    -rgba
    -rgb
    -alpha}

proc box {name args} {
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


set_help_text create_power_bar\
{The command 'osd_widgets::create_power_bar' supports the following parameters:
    -name == Name of the power bar
    -w == Width of the power bar (in pixels)
    -h == Height of the power bar
    -barcolor == Powerbar color
    -background == When power declines this color is shown
    -edgecolor == This is the edge color (try white when I doubt which color to use)

Colors must have the following hexadecimal format 0xRRGGBBAA.

The power bar is initially created outside the viewable area, so we need to invoke
the 'update_power_bar' command to make it visible. Use 'hide_power_bar' to
remove it again.}

set_help_text update_power_bar\
{The command 'update_power_bar' uses the following parameters:
    -name == Name of the power bar
    -x == vertical position of the power bar
    -y == horizontal position of the power bar
    -power == fill rate of the power bar in decimal percentages (10% == 0.1)
    -text == text to be printed above the power bar}

proc create_power_bar {name w h barcolor background edgecolor} {
	osd create rectangle $name        -rely 999 -relw $w -relh $h -rgba $background
	osd create rectangle $name.top    -x -1 -y   -1 -relw 1 -w 2 -h 1 -rgba $edgecolor
	osd create rectangle $name.bottom -x -1 -rely 1 -relw 1 -w 2 -h 1 -rgba $edgecolor
	osd create rectangle $name.left   -x -1         -w 1 -relh 1      -rgba $edgecolor
	osd create rectangle $name.right  -relx 1       -w 1 -relh 1      -rgba $edgecolor
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


set_help_text toggle_fps \
{Enable/disable a frames per second indicator in the top-left corner of the screen.}

variable fps_after

proc toggle_fps {} {
	variable fps_after
	if [info exists fps_after] {
		after cancel $osd_widgets::fps_after
		osd destroy fps_viewer
		unset fps_after
	} else {
		osd create rectangle fps_viewer -x 5 -y 5 -z 0 -w 63 -h 20 -rgba 0x00000080
		osd create text fps_viewer.text -x 5 -y 3 -z 1 -rgba 0xffffffff
		proc fps_refresh {} {
			variable fps_after
			osd configure fps_viewer.text -text [format "%2.1fFPS" [openmsx_info fps]]
			set fps_after [after realtime .5 [namespace code fps_refresh]]
		}
		fps_refresh
	}
	return ""
}

set_help_text osd_widgets::box\
{The command 'osd_widgets::text_box' supports the same parameters as an 'osd_widgets::box' command.
With the following exception:

-text: defines the text to be printed, can have multiple lines (separated by 'new line' characters).
-textcolor: defines the color of the text
-textsize: defines the font size of the text}

proc text_box {name args} {
	# Default values in case nothing is given
	set txt_color 0xffffffff
	set txt_size 6

	# Process arguments
	set box_props [list]
	foreach {key val} $args {
		switch -- $key {
			-text      { set full_message $val }
			-textcolor { set txt_color    $val }
			-textsize  { set txt_size     $val }
			default    { lappend box_props $key $val }
		}
	}

	# For handheld devices set minimal text size to 9
	#  TODO: does this belong here or at some higher level?
	if {($::scale_factor == 1) && ($txt_size < 9)} {
		set txt_size 9
	}

	# Destroy widget (if it already existed)
	osd destroy $name

	if {$full_message == ""} return
	set message_list [split $full_message "\n"]
	set lines [llength $message_list]

	eval "osd_widgets::box \{$name\} $box_props -h [expr 4 + (($txt_size + 1) * $lines)]"

	set line 0
	foreach message $message_list {
		osd create text $name.$line -x 2 -size $txt_size -rgb $txt_color -text $message -y [expr 2 + ($line * ($txt_size + 1))]
		incr line
	}
}

# only export stuff that is useful in other scripts or for the console user
namespace export toggle_fps
namespace export msx_init
namespace export msx_update
namespace export box
namespace export text_box
namespace export create_power_bar
namespace export update_power_bar
namespace export hide_power_bar

};# namespace osd_widgets

# only import stuff to global that is useful outside of scripts (i.e. for the console user)
namespace import osd_widgets::toggle_fps

