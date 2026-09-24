set_help_text reload_rom \
{Reloads an inserted ROM from disk, preserving its slot, mapper and IPS patches,
then resets the MSX. With multiple ROMs, specify a slot, e.g. reload_rom carta.
Current gameplay is restarted; cartridge SRAM/Flash uses normal persistence.
}

proc reload_rom {{slot ""}} {
	set cartridges [list]
	foreach media [machine_info media] {
		if {![string match cart? $media]} continue
		set info [machine_info media $media]
		if {[dict exists $info type] && [dict get $info type] eq "rom"} {
			lappend cartridges $media
		}
	}
	if {$slot eq ""} {
		if {[llength $cartridges] == 0} {error "No ROM cartridge is inserted."}
		if {[llength $cartridges] != 1} {
			error "Multiple ROM cartridges are inserted. Specify a slot: reload_rom [join $cartridges { or reload_rom }]"
		}
		set slot [lindex $cartridges 0]
	} elseif {$slot ni $cartridges} {
		error "No ROM cartridge is inserted in $slot."
	}

	set info [machine_info media $slot]
	set filename [dict get $info target]
	set patches [dict get $info patches]
	# Catch absent/incomplete build outputs before removing the current ROM.
	foreach path [linsert $patches 0 $filename] {
		if {![file isfile $path] || ![file readable $path] || [file size $path] == 0} {
			error "Cannot reload: file is missing, empty or unreadable: $path"
		}
	}
	set command [list $slot insert $filename -romtype [dict get $info mappertype]]
	foreach patch $patches {lappend command -ips $patch}
	# Cartridge replacement destroys the old device (flushing its saves)
	# before constructing the new device and loading its persistent data.
	{*}$command
	reset
	message "Reloaded [file tail $filename] and reset."
	return $filename
}
