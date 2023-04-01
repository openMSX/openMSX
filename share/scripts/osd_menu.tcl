namespace eval osd_menu {

# default colors defined here, for easy global tweaking
variable default_bg_color "0x7090aae8 0xa0c0dde8 0x90b0cce8 0xc0e0ffe8"
variable default_text_color 0x000000ff
variable default_select_color "0x0044aa80 0x2266dd80 0x0055cc80 0x44aaff80"
variable default_header_text_color 0xff9020ff

# drop handler
bind_default filedrop osd_menu::drop_handler -event

# button stuff
variable button_fade_timeout 8
variable button_fadeout_time 4

user_setting create boolean osd_menu_button \
{Show a menu button to open the main menu, or disable this button.} true

proc setting_changed {name1 name2 op} {
	if {$::osd_menu_button} {;# setting changed from disabled to enabled
		add_button
	} else { ;# setting changed from enabled to disabled
		remove_button
	}
}

trace add variable ::osd_menu_button write [namespace code setting_changed]

# TODO: make this a generic proc with osd element as input?
proc is_cursor_on_button {} {
	set x 2; set y 2
	catch {lassign [osd info "main_menu_pop_up_button" -mousecoord] x y}
	return [expr {0 <= $x && $x <= 1 && 0 <= $y && $y <= 1}]
}

proc check_button_clicked {} {
	if {$::osd_menu_button} {
		if {[is_cursor_on_button]} {
			main_menu_toggle
		}
		after "mouse button1 down" [namespace code check_button_clicked]
	}
}

variable start_fadeout_button_id 0

proc update_button_fade {} {
	if {$::osd_menu_button} {
		variable start_fadeout_button_id
		if {[is_cursor_on_button]} {
			if {$start_fadeout_button_id ne "0"} {
				after cancel $start_fadeout_button_id
				osd configure "main_menu_pop_up_button" -fadeCurrent 1 -fadeTarget 1
				set start_fadeout_button_id 0
			}
		} else {
			if {$start_fadeout_button_id eq "0"} {
				variable button_fade_timeout
				set start_fadeout_button_id [after realtime $button_fade_timeout {
					if {$::osd_menu_button} {
						osd configure "main_menu_pop_up_button" -fadeTarget 0
					}
				}]
			}
		}
		after "mouse motion" [namespace code update_button_fade]
	}
}

proc add_button {} {
	variable default_bg_color
	variable default_text_color
	variable button_fadeout_time

	osd create rectangle main_menu_pop_up_button -z 0 -x 0 -y 0 -w 35 -h 16 -rgba $default_bg_color -fadePeriod $button_fadeout_time
	osd create text main_menu_pop_up_button.text -z 0 -x 0 -y 0 -rgba $default_text_color -text "menu"

	after "mouse button1 down" [namespace code check_button_clicked]
	update_button_fade
}

proc remove_button {} {
	osd destroy main_menu_pop_up_button
}

## Disabled for now, the imgui stuff should replace it.
## In the future we'll likely fully remove this.
#if {![regexp dingux "[openmsx_info platform]"] && $::osd_menu_button} {
#	add_button
#}

} ;# namespace osd_menu
