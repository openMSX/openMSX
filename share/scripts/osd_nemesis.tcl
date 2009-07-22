namespace eval osd_nemesis {

variable scriptlocation [info script]

# todo: help text

	proc toggle_nemesis_1_shield {} {

		if {![catch {osd info nem -rgba} errmsg]} {
			osd destroy nem
			return ""
		}

		osd_widgets::msx_init nem
		osd create rectangle nem.shield  \
			-alpha 0 -fadeTarget 0 -fadePeriod 2 \
			-image [data_file scripts/shield.png]

		create_shield
	}

	proc clip {min max val} {
		expr {($val < $min) ? $min : (($val > $max) ? $max : $val)}
	}

	proc create_shield {} {

		if {[catch {osd info nem -rgba} errmsg]} {
			return ""
		}
		osd_widgets::msx_update nem

		# vic viper location
		set x [peek 0xe206]
		set y [peek 0xe204]
		osd configure nem.shield -relx [expr $x - 9] -rely [expr $y - 9]

		for {set i 0} {$i < 32} {incr i} {

			# set base address
			set addr [expr 0xe300 + ($i*0x20)]

			# get enemy id
			set id [peek $addr]

			# enegry pod / flash pod / life pod / score pod /
			# endless score pod / explosion / end brain connections
			if {(17 <= $id) && ($id <= 26) && ($id != 20)} continue

			set a [peek [expr $addr + 6]]
			set b [peek [expr $addr + 4]]

			# Not in contact with shield? Then do nothing.

			if {(pow($a - ($x+6), 2) + pow($b - ($y + 7), 2)) >= 961} continue

			# ground turrets change into razor discs :)
			if {$id == 1 && $i < 16} {poke $addr 2}

			# homing ships and walkers
			if {$id == 4 || $id == 5} {poke $addr 21}

			# change color of sprite when in contact with shield
			# poke [expr $addr + 13] 15

			# Shield routine (hit front/back/top/bottom)
			set shieldstrength 2
			set dx [expr ($x > $a) ? -$shieldstrength : $shieldstrength]
			set dy [expr ($y > $b) ? -$shieldstrength : $shieldstrength]
			set xn [expr $a + $dx]
			set yn [expr $b + $dy]
			poke [expr $addr + 6] [clip 0 255 $xn]
			poke [expr $addr + 4] [clip 0 255 $yn]
			poke [expr $addr + 8] [expr $dy & 255]
			poke [expr $addr + 10] [expr $dx & 255]

			# make shield visible
			osd configure nem.shield -alpha 0x80
		}
		after frame [namespace code create_shield]
	}

	namespace export toggle_nemesis_1_shield

} ;# namespace osd_nemesis

namespace import osd_nemesis::*
