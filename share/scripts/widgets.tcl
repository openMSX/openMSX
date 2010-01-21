namespace eval debug_widgets {

#TODO: Help Texts

proc toggle_show_palette {} {

	if {![catch {osd info palette_viewer -rgba} errmsg]} {
		osd destroy palette_viewer
		return ""
	}

	osd_widgets::box palette_viewer -x 4 -y 4 -w 56 -h 194 -rgba 0xffffffff -fill 0x00000080

	for {set i 0} {$i < 16} {incr i} {
		osd_widgets::box palette_viewer.$i \
			-x 20 \
			-y [expr ($i*12)+2] \
			-w 10 \
			-h 10 \
			-rgba 0xffffffff \
			-fill 0xff0000ff
		osd create text palette_viewer.$i.text \
			-x -16 \
			-rgba 0xffffffff \
			-size 10 \
			-text ""
	}
	
	update_palette
	return ""
}

proc update_palette {} {
	if {[catch {osd info palette_viewer -rgba} errmsg]} {
		return ""
	}

	for {set i 0} {$i < 16} {incr i} {
		set color [getcolor $i]

		set r [string range $color 0 0]
		set g [string range $color 1 1]
		set b [string range $color 2 2]
		
		set rgbval [expr ($r << (5 + 16)) + ($g << (5 + 8)) + ($b << 5)]
		osd configure palette_viewer.$i -rgb $rgbval
		osd configure palette_viewer.$i.text -text "[format %02d $i]     $color"
	}
	after frame [namespace code update_palette]
	return ""
}

proc toggle_vdp_reg_viewer {} {

	if {![catch {osd info vdp_reg_viewer -rgba} errmsg]} {
		osd destroy vdp_reg_viewer
		osd destroy vdp_statreg_viewer
		return ""
	}

	set fontsize 9

	# note: this method of VDP detection will fail on e.g. MSX1 machines with V9938
	set vdpreg [expr ([debug read slotted\ memory 0x2d]) ? 47 : 8]
	set vdpsta [expr ([debug read slotted\ memory 0x2d]) ? 10 : 1]

	osd create rectangle vdp_reg_viewer \
		-x 0 \
		-y 0 \
		-h 480 \
		-w [expr $fontsize * 8] \
		-rgba 0x00000080
	osd create rectangle vdp_statreg_viewer \
		-x [expr ($fontsize * 8) + 16] \
		-y 0 \
		-h 480 \
		-w [expr $fontsize * 8] \
		-rgba 0x00000080

	for {set i 0} {$i < $vdpreg} {incr i} {
		osd create rectangle vdp_reg_viewer.indi$i \
			-x 0 \
			-y [expr $i * $fontsize] \
			-w [expr $fontsize * 8] \
			-h $fontsize \
			-rgba 0xff0000ff \
			-fadeTarget 0 \
			-fadePeriod 1
		osd create text vdp_reg_viewer.labl$i \
			-x 0 \
			-y [expr $i * $fontsize] \
			-size $fontsize \
			-text "R# [format %02d $i]:" \
			-rgba 0xffffffff
		osd create text vdp_reg_viewer.stat$i \
			-x [expr $fontsize * 4] \
			-y [expr $i * $fontsize] \
			-size $fontsize \
			-text "[format 0x%02X [debug read VDP\ regs $i]]" \
			-rgba 0xffffffff
	}
	
	for {set i 0} {$i < $vdpsta} {incr i} {
		osd create rectangle vdp_statreg_viewer.indi$i \
			-x 0 \
			-y [expr $i * $fontsize] \
			-w [expr $fontsize * 8] \
			-h [expr $fontsize] \
			-rgba 0xff0000ff \
			-fadeTarget 0 \
			-fadePeriod 1
		osd create text vdp_statreg_viewer.labl$i \
			-x 0 \
			-y [expr $i * $fontsize] \
			-size $fontsize \
			-text "S# [format %02d $i]:" \
			-rgba 0xffffffff
		osd create text vdp_statreg_viewer.stat$i \
			-x [expr $fontsize * 4] \
			-y [expr $i * $fontsize] \
			-size $fontsize \
			-text "[format 0x%02X [debug read VDP\ status\ regs $i]]" \
			-rgba 0xffffffff
	}
	update_vdp_reg_viewer
}

proc update_vdp_reg_viewer {} {

	if {[catch {osd info vdp_reg_viewer -rgba} errmsg]} {
		return ""
	}

	# note: this method of VDP detection will fail on e.g. MSX1 machines with V9938
	set vdpreg [expr ([debug read slotted\ memory 0x2d]) ? 47 : 8]
	set vdpsta [expr ([debug read slotted\ memory 0x2d]) ? 10 : 1]

	for {set i 0} {$i < $vdpreg} {incr i} {
		set vdp_stat "[format 0x%02X [debug read VDP\ regs $i]]"
		if {$vdp_stat != [osd info vdp_reg_viewer.stat$i -text]} {
			osd configure vdp_reg_viewer.stat$i -text "$vdp_stat"
			osd configure vdp_reg_viewer.indi$i -fadeCurrent 1
		}
	}
	
	for {set i 0} {$i < $vdpsta} {incr i} {
		set vdp_stat "[format 0x%02X [debug read VDP\ status\ regs $i]]"
		if {$vdp_stat != [osd info vdp_statreg_viewer.stat$i -text]} {
			osd configure vdp_statreg_viewer.stat$i -text "$vdp_stat"
			osd configure vdp_statreg_viewer.indi$i -fadeCurrent 1
		}
	}

	after frame [namespace code update_vdp_reg_viewer]
		return ""
	}

### heavy WIP

proc toggle_cheat_finder {} {

	osd create rectangle cheats -x 0 -y 0 -w 120 -h 200 -rgba 0x00000080

	osd create rectangle cheats.head1 -x 1 -y 1 -w 35 -h 10 -rgba 0xff000080
	osd create text cheats.head1.text -size 8 -text "Address" -rgba 0xffffffff

	osd create rectangle cheats.head2 -x 37 -y 1 -w 20 -h 10 -rgba 0xff000080
	osd create text cheats.head2.text -size 8 -text "Cur" -rgba 0xffffffff

	osd create rectangle cheats.head3 -x 58 -y 1 -w 20 -h 10 -rgba 0xff000080
	osd create text cheats.head3.text -size 8 -text "Pre" -rgba 0xffffffff

	osd create rectangle cheats.head4 -x 79 -y 1 -w 40 -h 10 -rgba 0xff000080
	osd create text cheats.head4.text -size 8 -text "Real" -rgba 0xffffffff

	for {set i 0} {$i < 17} {incr i} {
		osd create rectangle cheats.addr$i -x 1 -y [expr $i * 11 + 12] -w 35 -h 10 -rgba 0xf0000080
		osd create text cheats.addr$i.text -size 8 -text "" -rgba 0xffffffff

		osd create rectangle cheats.pre$i -x 37 -y [expr $i * 11 + 12] -w 20 -h 10 -rgba 0xf0000080
		osd create text cheats.pre$i.text -size 8 -text "" -rgba 0xffffffff

		osd create rectangle cheats.aft$i -x 58 -y [expr $i * 11 + 12] -w 20 -h 10 -rgba 0xf0000080
		osd create text cheats.aft$i.text -size 8 -text "" -rgba 0xffffffff

		osd create rectangle cheats.real$i -x 79 -y [expr $i * 11 + 12] -w 40 -h 10 -rgba 0xf0000080
		osd create text cheats.real$i.text -size 8 -text "" -rgba 0xffffffff
	}
}

proc update_cheat_finder {} {
	set cheats [split [[string trim findcheat]] "\n"]

	for {set i 0} {$i < 17} {incr i} {
		
		if {$i > [expr [llength $cheats] - 2]} {
			osd configure cheats.addr$i.text -text ""
			osd configure cheats.real$i.text -text ""
		
		} else {
			set line [lindex $cheats $i]
			set addr [string range $line 0 5]
			osd configure cheats.addr$i.text -text $addr
			osd configure cheats.real$i.text -text [peek $addr]
		}
	}
	after frame update_cheat_finder
}

namespace export toggle_show_palette
namespace export toggle_vdp_reg_viewer

} ;# namespace debug_widgets

namespace import debug_widgets::*
