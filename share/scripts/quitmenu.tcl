namespace eval quitmenu {

	proc quit_menu {} {
		set items [list "No" "Yes"]
		set menu_def \
			{ execute quitmenu::get_choice
				font-size 8
				border-size 2
				width 100
				xpos 100
				ypos 100
				header { text "Quit openMSX?"
						font-size 10
						post-spacing 6 }}

		osd_menu::do_menu_open [osd_menu::prepare_menu_list $items [llength $items] $menu_def]
		activate_input_layer quit_menu
	}

	proc get_choice {item} {
		osd_menu::menu_close_all
		if {$item eq "Yes"} {::exit}
	}
	
namespace export quit_menu
}

namespace import quitmenu::*