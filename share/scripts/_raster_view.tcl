namespace eval raster_view {

variable raster_view_enabled false
variable raster_view_after_id 0

set_help_text toggle_raster_view \
"Shows a red square around the current position of the raster beam in debug
breaked state, as well as a message indicating the current position."

proc toggle_raster_view {} {
	variable raster_view_enabled
	variable raster_view_after_id

	if {$raster_view_enabled} {
		set raster_view_enabled false
		osd destroy showraster
		catch { after cancel $raster_view_after_id }
		return "Show raster disabled"
	} else {
		set raster_view_enabled true
		osd_widgets::msx_init showraster

		osd create rectangle showraster.box -x -100 -y -100 -h 3 -w 3 -rgba 0xFF1111FF

		set raster_view_after_id [after break [namespace code update_raster]]
		return "Show raster enabled: will indicate raster beam with red square at next break..."
	}
}

proc update_raster {} {
	variable raster_view_enabled
	variable raster_view_after_id
	if {$raster_view_enabled} {
		osd_widgets::msx_update showraster
		set mode [get_screen_mode_number]
		set x256 true
		if {$mode == 0} {
			set x256 [expr {!([debug read "VDP regs" 0] & 0x04)}]
		} elseif {$mode == 6 || $mode == 7} {
			set x256 false
		}
		set x [machine_info [expr {$x256 ? "VDP_msx_x256_pos" : "VDP_msx_x512_pos"}]]
		set y [machine_info VDP_line_in_frame]
		osd configure showraster.box -x $x -y $y
		message "Raster beam currently at: ($x, $y)"
		set raster_view_after_id [after break [namespace code update_raster]]
	}
}

namespace export toggle_raster_view

};# namespace raster_view

namespace import raster_view::*

