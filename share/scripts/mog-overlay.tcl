namespace eval mog_overlay {

variable num_enemies 16
variable num_rocks 8
variable mog_overlay_active false
variable item_cache
variable tomb_cache
variable ladder_cache
variable wall_cache
variable demon_cache
variable ep

# Demon summon spells
variable spell [list "" "YOMAR" "ELOHIM" "HAHAKLA" "BARECHET" "HEOTYMEO" \
                     "LEPHA" "NAWABRA" "ASCHER" "XYWOLEH" "HAMALECH"]
# Demon Max Power
variable demon [list 24 40 64 112 72 56 64 80 80 248]
# Item names
variable items [list "" "Arrow" "Ceramic Arrow" "Rolling Fire" "Fire" "Mine" \
                     "Magnifying Glass" "Holy Water" "Cape" "Magic Rod" \
                     "World Map" "Cross" "Great Key" "Necklace" "Crown" \
                     "Helm" "Oar" "Boots" "Doll" "Halo" "Bell" "Halo" "Candle" \
                     "Armor" "Carpet" "Helmet" "Lamp" "Vase" "Pendant" \
                     "Earrings" "Bracelet" "Ring" "Bible" "Harp" "Triangle" \
                     "Trumpet Shell" "Pitcher" "Sabre" "Dagger" "Feather" \
                     "Bronze Shield" "Bread and Water" "Salt" "Silver Shield" \
                     "Gold Shield" "Quiver of Arrows" "Coins" "Keys"]

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
	variable ep

	osd create rectangle mog -scaled true

	for {set i 0} {$i < $num_enemies} {incr i} {
		__create_power_bar mog.powerbar$i 0 999 16 4 0xff0000ff 0x00000080 0xffffffff
	}

	__create_power_bar mog.item 0 999 18 18 0x00000000 0xff770020 0xffffffff
	set item_cache 0

	osd create rectangle mog.tomb -x 0 -y 999 -w 16 -h 16 -rgba 0x00000080
	osd create text mog.tomb.text -x 1 -y 1 -size 5 -rgb 0xffffff
	set tomb_cache 0

	osd create rectangle mog.ladder -x 0 -y 999 -w 16 -h 1 -rgba 0xffffff80
	set ladder_cache 0

	osd create rectangle mog.wall -x 0 -y 999 -w 8 -h 32 -rgba 0x00ffff80
	set wall_cache 0

	osd create rectangle mog.wall2 -x 0 -y 999 -w 32 -h 24 -rgba 0x00000080
	osd create text mog.wall2.text -x 1 -y 1 -size 5 -rgb 0xffffff

	for {set i 0} {$i < $num_rocks} {incr i} {
		osd create rectangle mog.rock$i  -x 0 -y 999 -w 24 -h 4 -rgba 0x0000ff80
		osd create text mog.rock$i.text -x 0 -y 0 -size 4 -rgb 0xffffff
	}

	__create_power_bar mog.demon 0 999 49 5 0x2bdd2bff 0x00000080 0xffffffff
	set demon_cache 0

	#set field
#	for {set y 0} {$y<20} {incr y} {
#		for {set x 0} {$x<32} {incr x} {
#			osd create rectangle chr[expr $x+($y*32)] -x [expr ($x*8)+32] -y [expr ($y*8)+56] -h 8 -w 8 -rgba 0xffffff00
#		}
#	}
	
	# Enemy Power
	for {set i 0} {$i < 256} {incr i} {set ep($i) 1}

}

# Update the overlays
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

	for {set i 0; set addr 0xe800} {$i < $num_enemies} {incr i; incr addr 0x20} {
		set enemy_type [peek $addr]
		set enemy_hp [peek [expr $addr + 0x10]]
		if {$enemy_type > 0 && $enemy_hp > 0 && $enemy_hp < 200} {
			# Record Highest Power for an enemy
			if {$ep($enemy_type) < $enemy_hp} {set ep($enemy_type) $enemy_hp}

			set pos_x [expr 40 + [peek [expr $addr + 7]]]
			set pos_y [expr 18 + [peek [expr $addr + 5]]]
			set power [expr 1.00 * $enemy_hp / $ep($enemy_type)]
			set text [format "(%d): #%02X %d %d" $i $enemy_type $enemy_hp $ep($enemy_type)]
			update_power_bar mog.powerbar$i $pos_x $pos_y $power $text
		} else {
			hide_power_bar mog.powerbar$i
		}
	}

	set new_item [peek 0xe740]
	if {$new_item != $item_cache} {
		set item_cache $new_item
		if {$item_cache} {
			set height [expr ($item_cache < 7) ? 9 : 17]
			osd configure mog.item.left -h $height
			osd configure mog.item.right -h $height
			osd configure mog.item.bottom -y $height
			osd configure mog.item.text -text [lindex $items $item_cache]]
			osd configure mog.item -x [expr 31 + [peek 0xe743]] \
			                       -y [expr 23 + [peek 0xe742]]
		} else {
			osd configure mog.item -y 999
		}
	}

	set new_tomb [peek 0xe678]
	if {$new_tomb != $tomb_cache} {
		set tomb_cache $new_tomb
		if {($tomb_cache > 0) && ($tomb_cache <= 10)} {
			osd configure mog.tomb.text -text "[lindex $spell $tomb_cache]"
			osd configure mog.tomb -x [expr 32 + [peek 0xe67a]] \
			                       -y [expr 24 + [peek 0xe679]]
		} else {
			osd configure mog.tomb -y 999
		}
	}

	#todo: see what the disappear trigger is (up, down, either)
	set new_ladder [expr [peek 0xec00] | [peek 0xec07]]
	if {$new_ladder != $ladder_cache} {
		set ladder_cache $new_ladder
		if {$ladder_cache != 0} {
			osd configure mog.ladder -x [expr 32 + [peek 0xec02]] \
			                         -y [expr 24 + [peek 0xec01]] \
			                         -h [expr 8 * [peek 0xec03]]
		} else {
			osd configure mog.ladder -y 999
		}
	}

	set new_wall [peek 0xeaa0]
	if {$new_wall != $wall_cache} {
		set wall_cache $new_wall
		if {$wall_cache != 0} {
			osd configure mog.wall -x [expr 32 + [peek 0xeaa3]] \
			                       -y [expr 24 + [peek 0xeaa2]]
		} else {
			osd configure mog.wall -y 999
		}
	}

	if {[peek 0xea20] != 0} {
		set text [format "-: -- (%02X)" [peek 0xea23]]
		osd configure mog.wall2 -x [expr 32 + [peek 0xea22]] \
		                        -y [expr 24 + [peek 0xea21]]
		osd configure mog.wall2.text -text $text
	} else {
		osd configure mog.wall2 -y 999
	}

	for {set i 0; set addr 0xec30} {$i < $num_rocks} {incr i; incr addr 8} {
		if {[peek $addr] > 0} {
			set pos_x [expr 40 + [peek [expr $addr + 3]]]
			set pos_y [expr 18 + [peek [expr $addr + 2]]]
			set rock_hp [peek [expr $addr + 6]]
			set text [format "$i: -- (%02X)" $rock_hp]
			osd configure mog.rock$i -x $pos_x -y $pos_y
			osd configure mog.rock$i.text -text $text
		} else {
			osd configure mog.rock$i -y 999
		}
	}

	if {([peek 0xe0d2] != 0) && ([peek 0xe710] != 0)} {
		osd configure mog.demon -x 143 -y 49
		print_mog_text 1 1 "Now Fighting [lindex $spell [expr [peek 0xe041] - 1]]"
		set new_demon [peek 0xe710]
		if {$new_demon != $demon_cache} {
			set demon_cache new_demon
			set demon_max [lindex $demon [expr [peek 0xe041] - 2]]
			if {$demon_max > 0} {
				osd configure mog.demon.bar -w [expr (48 * (($demon_cache * ([peek 0xe076] + 1)) / $demon_max.0))]
			}
		}
	} else {
		osd configure mog.demon -y 999
	}

#	for {set y 0} {$y<20} {incr y} {
#		for {set x 0} {$x<32} {incr x} {
#			if {[peek [expr (0xed80+$x)+($y*32)]]==108} {osd configure chr[expr $x+($y*32)] -alpha 128} else {osd configure chr[expr $x+($y*32)] -alpha 0}
#		}
#	}

	after frame [namespace code update_overlay]
}

proc print_mog_text {x y text} {

	set text [string tolower $text]

	for {set i 0} {$i < [string length $text]} {incr i} {
		scan [string range $text $i $i]] "%c" ascii
		set ascii [expr $ascii - 86]
		if {$ascii >= -38 && $ascii <= -29} {set ascii [expr $ascii + 39]}
		if {$ascii < 0} {set ascii 0}
		poke [expr (0xed80 + $x) + ($y * 32) + $i] $ascii
	}
}

proc toggle_mog_overlay {} {
	variable mog_overlay_active
	variable num_enemies

	if {$mog_overlay_active} {
		set mog_overlay_active false
		osd destroy mog
	} else {
		set mog_overlay_active true
		init
		update_overlay
	}
}

namespace export toggle_mog_overlay

};# namespace mog_overlay

namespace import mog_overlay::*


# these procs are generic and are supposed to be moved to a generic OSD library or something similar.

proc __create_power_bar {name x y w h barcolor background edgecolor} {
	osd create rectangle $name        -x $x -y $y -w $w -h $h -rgba $background
	osd create rectangle $name.top    -x 0 -y 0 -w $w -h 1 -rgba $edgecolor
	osd create rectangle $name.left   -x 0 -y 0 -w 1 -h $h -rgba $edgecolor
	osd create rectangle $name.right  -x $w -y 0 -w 1 -h $h -rgba $edgecolor
	osd create rectangle $name.bottom -x 0 -y $h -w [expr $w + 1] -h 1 -rgba $edgecolor
	osd create rectangle $name.bar    -x 1 -y 1 -w [expr $w - 1] -h [expr $h - 1] -rgba $barcolor
	osd create text      $name.text   -x 0 -y -6 -size 4 -rgba $edgecolor
}

proc update_power_bar {name x y power text} {
	if {$power > 1.0} {set power 1.0}
	if {$power < 0.0} {set power 0.0}
	osd configure $name -x $x -y $y 
	osd configure $name.bar -w [expr ([osd info $name -w] - 1) * $power]
	osd configure $name.text -text "$text"
}

proc hide_power_bar {name} {
	osd configure $name -y 999
}
