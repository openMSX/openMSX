namespace eval quitmenu {

	proc list_quit {} {
		set slots [list]
			lappend slots "No"
			lappend slots "Yes"
		return $slots
	}

	proc menu_create_quit_menu {} {
		set items [list_quit]
		set menu_def \
			{ execute quitmenu::get_quit
				font-size 8
				border-size 2
				width 100
				xpos 100
				ypos 100
				header { text "Quit openMSX?"
						font-size 10
						post-spacing 6 }}

		return [osd_menu::prepare_menu_list $items 2 $menu_def]
	}

	proc get_quit {item} {
		osd_menu::menu_close_all
		if {$item eq "Yes"} {::quit}
	}

	proc quit_menu {} {
		osd_menu::do_menu_open [menu_create_quit_menu]
		for {set i 0} {$i < [llength [list_quit]]} {incr i} {
			bind -layer quit_menu "$i" "quitmenu::set_quit [lindex [quitmenu::list_quit] $i]; tas::unbind_number_keys"
		}
		activate_input_layer quit_menu
	}

namespace export quit_menu
}

namespace import quitmenu::*