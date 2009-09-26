proc tk_menu_test {} {

	return 'this will only work when TK is working properly'

	wm title $main "Tcl/Kt Catapult"

	#set new top for container - Cartridges
	set cart_label [ttk::labelframe $main.lbl_cart -text "Cartridge Slots"]
	ttk::button $cart_label.load_carta -text "Load Cart A" -command "load_media carta;reset" 
	ttk::button $cart_label.load_cartb -text "Load Cart B" -command "load_media cartb;reset" 
	ttk::entry $cart_label.load_carta_entry -width 30 -textvariable carta
	ttk::entry $cart_label.load_cartb_entry -width 30 -textvariable cartb

	grid $cart_label -column 0 -row 2
	grid $cart_label.load_carta_entry -column 0 -row 0
	grid $cart_label.load_cartb_entry -column 0 -row 1
	grid $cart_label.load_carta -column 1 -row 0
	grid $cart_label.load_cartb -column 1 -row 1 

	#set new top for container - diskridges
	set disk_label [ttk::labelframe $main.lbl_disk -text "Disk Slots"]
	ttk::button $disk_label.load_diska -text "Load Disk A" -command "load_media diska" 
	ttk::button $disk_label.load_diskb -text "Load Disk B" -command "load_media diskb" 
	ttk::entry $disk_label.load_diska_entry -width 30 -textvariable diska
	ttk::entry $disk_label.load_diskb_entry -width 30 -textvariable diskb

	grid $disk_label -column 0 -row 0
	grid $disk_label.load_diska_entry -column 0 -row 0
	grid $disk_label.load_diskb_entry -column 0 -row 1
	grid $disk_label.load_diska -column 1 -row 0
	grid $disk_label.load_diskb -column 1 -row 1 

	set lbl_cas [ttk::labelframe $main.lbl_cas -text "Cassette"]
	ttk::button $lbl_cas.load_cas -text "Cassette" -command "load_media cassette" 
	ttk::entry $lbl_cas.load_cas_entry -width 30

	grid $lbl_cas -column 0 -row 4
	grid $lbl_cas.load_cas_entry -column 0 -row 0
	grid $lbl_cas.load_cas -column 1 -row 0
}

proc load_media { media } {
		
	global carta
	global cartb
	global diska
	global diskb
		
	set pause on
		
	set types {
		{{All Files} * }
	}
	
	set media_selector [string range $media 0 3] 
	
	#load a rom
	if {$media_selector=="cart"} {
		set types {
			{{ROM Files}	{.ROM .ZIP .GZ}}
			{{All Files}	* }
		}
	}

	#load a disk
	if {$media_selector=="disk"} {
		set types {
			{{Disk Files}	{.DSK .ZIP .GZ}}
			{{All Files}	* }
		}
	}

	#load a cassette
	if {$media_selector=="cass"} {
		set types {
			{{CAS Files}	{.WAV .CAS .ZIP .GZ}}
			{{All Files}	* }
		}
	}

	set filename [tk_getOpenFile -filetypes $types]
	set pause off
	
	if {$filename==""} {return}
	if {![file exists $filename]} {tk_messageBox -message "File Not Found: $filename" -title "I am Error";return}
	set $media $filename
	eval {$media eject;$media $filename}
}