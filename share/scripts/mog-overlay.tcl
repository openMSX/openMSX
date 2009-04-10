namespace eval mog_overlay {

variable num_enemies
variable num_rocks
variable mog_overlay_active false
variable item_cache
variable tomb_cache
variable ladder_cache
variable wall_cache
variable demon_cache

#Demon summon spells
set spell [list "YOMAR" "ELOHIM" "HAHAKLA" "BARECHET" "HEOTYMEO" "LEPHA" "NAWABRA" "ASCHER" "XYWOLEH" "HAMALECH"]
#Demon Max Power
set demon [list 24 40 64 112 72 56 64 80 80 248]
#Item list
set items [list "Arrow" "Ceramic Arrow" "Rolling Fire" "Fire" "Mine" "Magnifying Glass" "Holy Water" "Cape" \
	   "Magic Rod" "World Map" "Cross" "Great Key" "Necklace" "Crown" "Helm" "Oar" "Boots" "Doll" "Halo" \
	   "Bell" "Halo" "Candle" "Armor" "Carpet" "Helmet" "Lamp" "Vase" "Pendant" "Earrings" "Bracelet" "Ring" \
	   "Bible" "Harp" "Triangle" "Trumpet Shell" "Pitcher" "Sabre" "Dagger" "Feather" "Bronze Shield" \
	   "Bread and Water" "Salt" "Silver Shield" "Gold Shield" "Quiver of Arrows" "Coins" "Keys"]

#Enemy Power
for {set i 0} {$i < 256} {incr i} {set ep($i) 1}

#Init Overlays
proc init {} {
	variable num_enemies
	variable num_rocks
	variable label_color
	variable label_text_color
	variable item_cache
	variable tomb_cache
	variable ladder_cache
	variable wall_cache
	variable demon_cache

	set num_enemies 16
	set num_rocks 8

	for {set i 0} {$i < $num_enemies} {incr i} {
		__CreatePowerBar mog_powerbar$i 240 0 16 4 0xff0000ff 0x00000080 0xffffffff
	}

	__CreatePowerBar mog_item 0 240 18 18 0x00000000 0xff770020 0xffffffff
	set item_cache 0

	osd create rectangle mog_tomb -x 0 -y 240 -w 16 -h 16 -rgba 0x00000080 -scaled true
	osd create text mog_tomb.text -x 1 -y 1 -size 5 -rgba 0xffffffff -scaled true
	set tomb_cache 0

	osd create rectangle mog_ladder -x 0 -y 240 -w 16 -h 1 -rgba 0xffffff80 -scaled true
	set ladder_cache 0

	osd create rectangle mog_wall -x 0 -y 240 -w 8 -h 1 -rgba 0x00ffff80 -scaled true
	set wall_cache 0

	osd create rectangle mog_wall2 -x 0 -y 240 -w 32 -h 24 -rgba 0x00000080 -scaled true
	osd create text mog_wall2.text -x 1 -y 1 -size 5 -rgba 0xffffffff -scaled true

	for {set i 0} {$i < $num_rocks} {incr i} {
		osd create rectangle mog_rock$i  -x 0 -y 240 -w 24 -h 4 -rgba 0x0000ff80 -scaled true
		osd create text mog_rock$i.text -x 0 -y 0 -size 4 -rgba 0xffffffff -scaled true
	}

	__CreatePowerBar mog_demon 0 240 49 5 0x2bdd2bff 0x00000080 0xffffffff
	set demon_cache 0

	#set field
#	for {set y 0} {$y<20} {incr y} {
#		for {set x 0} {$x<32} {incr x} {
#			osd create rectangle chr[expr $x+($y*32)] -x [expr ($x*8)+32] -y [expr ($y*8)+56] -h 8 -w 8 -rgba 0xffffff00 -scaled true
#		}
#	}
}

#Destroy overlays
proc deinit {} {
	variable num_enemies
	variable num_rocks

	for {set i 0} {$i < $num_enemies} {incr i} {
		osd destroy mog_powerbar$i
	}
	osd destroy mog_item
	osd destroy mog_tomb
	osd destroy mog_ladder
	osd destroy mog_wall
	osd destroy mog_wall2
	for {set i 0} {$i < $num_rocks} {incr i} {
		osd destroy mog_rock$i
	}
	osd destroy mog_demon
}

#Update the overlays
proc update_overlay {} {
	variable mog_overlay_active
	variable num_enemies
	variable num_rocks
	variable label_color
	variable label_text_color
	variable item_cache
	variable tomb_cache
	variable ladder_cache
	variable wall_cache
	variable demon_cache
	variable spell
	variable demon
	variable items
	variable ep

	if {!$mog_overlay_active} return

	#set horizontal_stretch 320;# to prevent stretch messing up coords
	for {set i 0} {$i < $num_enemies} {incr i} {
		set enemy_type [peek [expr 0xe800 + $i * 0x0020]]
		set enemy_hp [peek [expr 0xe810 + $i * 0x0020]]

		if { $enemy_type > 0 && $enemy_hp>0 && $enemy_hp<200 } {
			set pos_x [expr 40+([peek [expr 0xe807 + (0x0020 * $i)]])]
			set pos_y [expr 18+([peek [expr 0xe805 + (0x0020 * $i)]])]
		} else {
			set pos_x 0
			set pos_y 240
		}

		#Record Highest Power for an enemy
		if {$ep($enemy_type)<$enemy_hp} {set ep($enemy_type) $enemy_hp}

		__UpdatePowerBar mog_powerbar$i $pos_x $pos_y [expr 1.00*$enemy_hp/$ep($enemy_type)] "($i): #[format %02X $enemy_type] $enemy_hp $ep($enemy_type)"

	}

	if {[peek 0xe740] != $item_cache} {
		set item_cache [peek 0xe740]
		if {$item_cache < 7} {
			osd configure mog_item.left -h 9
			osd configure mog_item.right -h 9
			osd configure mog_item.bottom -y 9
		} else {
			osd configure mog_item.left -h 17
			osd configure mog_item.right -h 17
			osd configure mog_item.bottom -y 17
		}
		if {$item_cache} {
			osd configure mog_item.text -text [lindex $items [expr $item_cache-1]]
			osd configure mog_item -x [expr 31+[peek 0xe743]] -y [expr 23+[peek 0xe742]]
		} else {
			osd configure mog_item -x 0 -y 240
		}
	}

	if {[peek 0xe678] != $tomb_cache} {
		set tomb_cache [peek 0xe678]
		if {($tomb_cache > 0) && ($tomb_cache <= 10)} {
			osd configure mog_tomb.text -text "[lindex $spell [expr $tomb_cache-1]]"
			osd configure mog_tomb -x [expr 32+[peek 0xe67a]] -y [expr 24+[peek 0xe679]]
		} else {
			osd configure mog_tomb -x 0 -y 240
		}
	}

	#todo: see what the disappear trigger is (up, down, either)
	if {[expr [peek 0xec00] | [peek 0xec07]] != $ladder_cache} {
		set ladder_cache [expr [peek 0xec00] | [peek 0xec07]]
		if {$ladder_cache != 0} {
			osd configure mog_ladder -x [expr 32+[peek 0xec02]] -y [expr 24+[peek 0xec01]] -h [expr 8*[peek 0xec03]]
		} else {
			osd configure mog_ladder -x 0 -y 240 -h 1
		}
	}

	if {[peek 0xeaa0] != $wall_cache} {
		set wall_cache [peek 0xeaa0]
		set wall_address [peek16 0xeaa5]
		if {$wall_cache != 0} {
			osd configure mog_wall -x [expr 32+[peek 0xeaa3]] -y [expr 24+[peek 0xeaa2]] -h 24
		} else {
			osd configure mog_wall -x 0 -y 240 -h 1
		}
	}

	if {[peek 0xea20] != 0} {
		osd configure mog_wall2 -x [expr 32+[peek 0xea22]] -y [expr 24+[peek 0xea21]]
		osd configure mog_wall2.text -text "-: -- ([format %02X [peek 0xea23]])"
	} else {
		osd configure mog_wall2 -x 0 -y 240
	}

	for {set i 0} {$i < $num_rocks} {incr i} {
		set rock_hp [peek [expr 0xec36 + $i * 8]]

		if {[peek [expr 0xec30 + $i * 8]] > 0} {
			set pos_x [expr 40+([peek [expr 0xec33 + (8 * $i)]])]
			set pos_y [expr 18+([peek [expr 0xec32 + (8 * $i)]])]
		} else {
			set pos_x 0
			set pos_y 240
		}
		osd configure mog_rock$i -x $pos_x -y $pos_y
		osd configure mog_rock$i.text -text "$i: -- ([format %02X $rock_hp])"
	}

	if {([peek 0xe0d2] != 0) && ([peek 0xe710] != 0)} {
		osd configure mog_demon -x 143 -y 49
		printMOGText 1 1 "Now Fighting [lindex $spell [expr [peek 0xe041] -2]]"
		if {[peek 0xe710] != $demon_cache} {
			set demon_cache [peek 0xe710]
			set demon_max [lindex $demon [expr [peek 0xe041] -2]]
			if {$demon_max > 0} {
				osd configure mog_demon.bar -w [expr (48*(($demon_cache*([peek 0xe076]+1))/$demon_max.0))]
			}
		}
	} else {
		osd configure mog_demon -x 0 -y 240
	}

#	for {set y 0} {$y<20} {incr y} {
#		for {set x 0} {$x<32} {incr x} {
#			if {[peek [expr (0xed80+$x)+($y*32)]]==108} {osd configure chr[expr $x+($y*32)] -alpha 128} else {osd configure chr[expr $x+($y*32)] -alpha 0}
#		}
#	}

	after frame [namespace code update_overlay]
}

proc toggle_mog_overlay {} {
	variable mog_overlay_active
	variable num_enemies

	if {$mog_overlay_active} {
		set mog_overlay_active false
		deinit
		unset num_enemies
	} else {
		set mog_overlay_active true
		init
		update_overlay
	}
}



namespace export toggle_mog_overlay

};# namespace mog_overlay

namespace import mog_overlay::*



proc printMOGText {x y text} {

set text [string tolower $text]

	for {set i 0} {$i<[string length $text]} {incr i} {
		scan [string range $text $i $i]] "%c" ascii
		set ascii [expr $ascii-86]
		if {$ascii>=-38 && $ascii<=-29} {set ascii [expr $ascii+39]}
		if {$ascii<0} {set ascii 0}
		poke [expr (0xed80+$x)+($y*32)+$i] $ascii
	}
}

proc __CreatePowerBar {name x y w h barcolor background edgecolor} {
	osd create rectangle $name 		-x $x -y $y -w $w -h $h -rgba $background -scaled true
	osd create rectangle $name.top 		-x 0 -y 0 -w $w -h 1 -rgba $edgecolor -scaled true
	osd create rectangle $name.left 	-x 0 -y 0 -w 1 -h $h -rgba $edgecolor -scaled true
	osd create rectangle $name.right	-x $w -y 0 -w 1 -h $h -rgba $edgecolor -scaled true
	osd create rectangle $name.bottom 	-x 0 -y $h -w [expr $w+1] -h 1 -rgba $edgecolor -scaled true
	osd create rectangle $name.bar 		-x 1 -y 1 -w [expr $w-1] -h [expr $h-1] -rgba $barcolor -scaled true
	osd create text	     $name.text 	-x 0 -y -6 -size 4 -rgba $edgecolor -scaled true
}

proc __UpdatePowerBar {name x y power text} {
	if {$power>1} {set power 1.00}
	if {$power<0} {set power 0.00}
	osd configure $name -x $x -y $y 
	osd configure $name.bar -w [expr ([osd info $name -w]-1)*$power]
	osd configure $name.text -text "$text"
}