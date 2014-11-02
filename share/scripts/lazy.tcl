# List of Tcl-scripts that can be loaded on-demand. For each script you also
# needs to provide a list of Tcl procs that it provides.
#  (preferably keep this list sorted on script name)
register_lazy "_about.tcl" about
register_lazy "_backwards_compatibility.tcl" {quit decr restoredefault alias}
register_lazy "_cheat.tcl" findcheat
register_lazy "_cashandler.tcl" {casload cassave caslist casrun caspos caseject tapedeck}
register_lazy "_cpuregs.tcl" {reg cpuregs get_active_cpu}
register_lazy "_cycle.tcl" {cycle cycle_back toggle}
register_lazy "_cycle_machine.tcl" {cycle_machine cycle_back_machine}
register_lazy "_disasm.tcl" {
	peek peek8 peek_u8 peek_s8 peek16 peek16_LE peek16_BE peek_u16
	peek_u16_LE peek_u16_BE peek_s16 peek_s16_LE peek_s16_BE
	poke poke8 poke16 poke16_LE poke16_BE dpoke disasm run_to step_over
	step_back step_out step_in step skip_instruction}
register_lazy "_example_tools.tcl" {get_screen listing get_color_count}
register_lazy "_filepool.tcl" {filepool get_paths_for_type}
register_lazy "_guess_title.tcl" {guess_title guess_rom_title}
register_lazy "_info_panel.tcl" toggle_info_panel
register_lazy "_mog-overlay.tcl" {toggle_mog_overlay toggle_mog_editor}
register_lazy "_monitor.tcl" monitor_type
register_lazy "_multi_screenshot.tcl" multi_screenshot
register_lazy "_music_keyboard.tcl" {toggle_music_keyboard}
register_lazy "_osd.tcl" {show_osd display_message is_cursor_in}
register_lazy "_osd_keyboard.tcl" toggle_osd_keyboard
register_lazy "_osd_menu.tcl" {
	main_menu_open main_menu_close main_menu_toggle
	do_menu_open prepare_menu_list menu_close_all select_menu_item}
register_lazy "_osd_nemesis.tcl" toggle_nemesis_1_shield
register_lazy "_osd_widgets.tcl" {
	toggle_fps msx_init msx_update box text_box create_power_bar
	update_power_bar hide_power_bar volume_control}
register_lazy "_psg_log.tcl" psg_log
register_lazy "_psg_profile.tcl" psg_profile
register_lazy "_quitmenu.tcl" quit_menu
register_lazy "_record_channels.tcl" {
	record_channels mute_channels unmute_channels solo}
register_lazy "_record_chunks.tcl" record_chunks
register_lazy "_reg_log.tcl" reg_log
register_lazy "_reverse.tcl" {
	reverse_prev reverse_next goto_time_delta go_back_one_step
	go_forward_one_step reverse_bookmarks auto_save_replay_enable
	auto_save_replay_disable auto_save_replay_set_filename
	toggle_reversebar enable_reversebar disable_reversebar auto_enable}
register_lazy "_rom_info.tcl" {rom_info getlist_rom_info}
register_lazy "_save_debuggable.tcl" {
	save_debuggable load_debuggable save_all load_all vramdump vram2bmp
	save_to_file}
register_lazy "_save_msx_screen.tcl" save_msx_screen
register_lazy "_savestate.tcl" {
	savestate loadstate delete_savestate list_savestates list_savestates_raw}
register_lazy "_scc_toys.tcl" {
	toggle_scc_editor toggle_psg2scc set_scc_wave toggle_scc_viewer}
register_lazy "_showdebuggable.tcl" {showdebuggable showmem}
register_lazy "_slot.tcl" {
	get_selected_slot slotselect get_mapper_size pc_in_slot watch_in_slot
	address_in_slot slotmap iomap}
register_lazy "_soundchip_utils.tcl" {
	get_num_channels get_volume_expr get_frequency_expr}
register_lazy "_soundlog.tcl" soundlog
register_lazy "_sprites.tcl" {sprite_viewer draw_matrix}
register_lazy "_stack.tcl" stack
register_lazy "_tas_tools.tcl" {
	toggle_frame_counter prev_frame next_frame start_of_frame
	advance_frame reverse_frame enable_tas_mode toggle_cursors ram_watch
	toggle_lag_counter reset_lag_counter}
register_lazy "_test_machines_and_extensions.tcl" {
	test_all_machines test_all_extensions}
register_lazy "_text_echo.tcl" text_echo
register_lazy "_tileviewer.tcl" {showtile showall}
register_lazy "_toggle_freq.tcl" toggle_freq
register_lazy "_trainer.tcl" trainer
register_lazy "_type_from_file.tcl" {type_from_file type_password_from_file}
register_lazy "_utils.tcl" {
	get_machine_display_name get_machine_display_name_by_config_name
	get_extension_display_name_by_config_name
	get_display_name_by_config_name get_machine_time format_time
	get_ordered_machine_list get_random_number clip file_completion
	filename_clean}
register_lazy "_vdp.tcl" {
	getcolor setcolor get_screen_mode get_screen_mode_number vdpreg vdpregs
	v9990regs vpeek vpoke palette}
register_lazy "_vdp_access_test.tcl" toggle_vdp_access_test
register_lazy "_vdp_busy.tcl" toggle_vdp_busy
register_lazy "_vdrive.tcl" vdrive
register_lazy "_vu-meters.tcl" toggle_vu_meters
register_lazy "_widgets.tcl" {toggle_show_palette toggle_vdp_reg_viewer}
