# MSX PICOVERSE PROJECT
# (c) 2026 Cristiano Goncalves
# The Retro Hacker
#
# picoverse2040.tcl - openMSX support for PicoVerse 2040 MultiROM UF2 images
#
# Lets openMSX run a MultiROM UF2 produced by 2040/software/multirom.pio/tool
# as if the PicoVerse 2040 cartridge were plugged into a cartridge slot. The
# Raspberry Pi Pico itself is not emulated; instead this script plays the role
# of the Pico firmware (pico/multirom/multirom.c):
#
#  1. The UF2 is decoded back into the Pico flash image:
#       [firmware][menu ROM 16KB][config area 16KB][ROM payloads...]
#  2. The 32KB menu (menu + config records) is inserted as a Page12 cartridge,
#     exactly as the firmware serves it at 0x4000-0xBFFF.
#  3. A write watchpoint on the menu's ROM select register (0x9D81) catches the
#     selection made by the user in the MSX menu.
#  4. The selected ROM is extracted from the flash image and inserted with the
#     openMSX mapper matching the PicoVerse mapper code, then the MSX is reset.
#     The embedded Nextor entries are mapped to generated openMSX extensions
#     (Sunrise IDE, and Sunrise IDE + 192KB memory mapper).
#
# A reset or a power cycle of the emulated MSX brings the menu back, and so
# does restarting openMSX with a PicoVerse entry still in the slot (restored
# setup). Loading a savestate keeps the entry that was running.
#
# The cartridge can be inserted in three ways:
#  - the "PicoVerse 2040 MultiROM" extension (share/extensions/PicoVerse_2040),
#    from the openMSX Extensions menu, the Cartridge Slot windows or '-ext';
#    it loads the UF2 in setting picoverse2040_uf2 (the last UF2 used)
#  - selecting the .uf2 file as a ROM image (Media > Cartridge Slot, '-carta',
#    'carta <file.uf2>'); the script notices the UF2 and takes over the slot
#  - the 'picoverse2040 insert <file.uf2>' console command
#
# Install with openmsx/install.ps1 or openmsx/install.sh, which copy the
# openmsx/share folder into the openMSX user directory (<openMSX user dir>/share).
# See docs/msx-picoverse-2040-openmsx.md in the repository for details.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version. See <https://www.gnu.org/licenses/>.

namespace eval picoverse2040 {

variable version "v2.65"

# Flash layout, must match pico/multirom/multirom.c and tool/src/multirom.c
variable flash_base       0x10000000
variable rp2040_family    0xE48BFF56
variable rom_name_max     50
variable record_size      59        ;# name(50) + mapper(1) + size(4) + offset(4)
variable max_records      128
variable menu_size        0x8000    ;# menu ROM (16KB) + config area (16KB)
variable config_offset    0x4000
variable monitor_addr     0x9D81    ;# ROM_SELECT_REGISTER in msx/src/menu.h
variable first_rom_offset 0x8000
variable search_limit     0x400000  ;# the firmware is far smaller than 4MB

# PicoVerse mapper code -> {description, openMSX romtype, fixed image size, bank size}
# Planar ROMs are padded with 0xFF to the window the firmware serves, which is
# what the MSX sees on real hardware for ROMs smaller than the window.
variable mappers [dict create \
	1  {PLA-16 Page12     32768 0}     \
	2  {PLA-32 Page12     32768 0}     \
	3  {KonSCC KonamiSCC  0     8192}  \
	4  {PLN-48 Page012    49152 0}     \
	5  {ASC-08 ASCII8     0     8192}  \
	6  {ASC-16 ASCII16    0     16384} \
	7  {Konami Konami     0     8192}  \
	8  {NEO-8  NEO-8      0     8192}  \
	9  {NEO-16 NEO-16     0     16384} \
	10 {SYSTEM nextor     0     0}     \
	11 {SYSTEM nextor_map 0     0}     \
	12 {ASC16X ASCII16-X  0     16384} \
	13 {PLN-64 Page0123   65536 0}     \
	14 {MANBW2 Manbow2    0     8192}  \
]

# Runtime state
variable image      ""      ;# flash image starting at the menu ROM (firmware stripped)
variable uf2_file   ""
variable records    [list]  ;# list of {name mapper size offset}
variable cache_dir  ""
variable slot       ""      ;# cartridge slot letter (a, b, ...)
variable slot_ps    0
variable slot_ss    "X"
variable hd_image   ""
variable state      "idle"  ;# idle | menu | rom
variable current    ""      ;# {cart <file>} or {ext <instance>}
variable menu_file  ""
variable selected   -1
variable wp_id      ""
variable pending    0
variable own_boot   0       ;# set while the reset that boots the selected entry is pending
variable ext_config "PicoVerse_2040"   ;# static extension in share/extensions
variable nextor_ext "PicoVerse2040_Nextor"  ;# prefix of the generated Nextor extensions
variable notified   ""                 ;# placeholder instance already reported
variable machine_id ""                 ;# machine last seen by on_machine_switch
variable watch_interval 0.25           ;# seconds between slot checks

user_setting create string picoverse2040_uf2 \
"PicoVerse 2040 MultiROM UF2 loaded when the PicoVerse 2040 extension is
inserted. Updated automatically every time a UF2 is inserted." ""

user_setting create string picoverse2040_slot \
"Cartridge slot (a, b, ...) used for the PicoVerse 2040 cartridge." a

user_setting create string picoverse2040_hd \
"Hard disk image used as the USB drive of the Nextor Sunrise IDE entries,
read every time a Nextor entry boots. Leave empty to use picoverse2040/hd.dsk
in the openMSX persistent folder." ""

# ---------------------------------------------------------------------------
# UF2 / flash image handling
# ---------------------------------------------------------------------------

proc read_binary {filename} {
	set fh [open $filename r]
	fconfigure $fh -translation binary
	set data [read $fh]
	close $fh
	return $data
}

proc write_binary {filename data} {
	set fh [open $filename w]
	fconfigure $fh -translation binary
	puts -nonewline $fh $data
	close $fh
}

# Decode a UF2 file into a flat flash image (starting at the flash base).
proc uf2_to_flash {filename} {
	variable flash_base
	variable rp2040_family

	set data [read_binary $filename]
	set len [string length $data]
	if {$len == 0 || $len % 512 != 0} {
		error "$filename is not a UF2 file (size is not a multiple of 512 bytes)"
	}

	set blocks [list]
	for {set o 0} {$o < $len} {incr o 512} {
		binary scan $data "@${o}iuiuiuiuiu" m0 m1 flags addr psize
		binary scan $data "@[expr {$o + 28}]iu" family
		binary scan $data "@[expr {$o + 508}]iu" mend
		if {$m0 != 0x0A324655 || $m1 != 0x9E5D5157 || $mend != 0x0AB16F30} {
			error "$filename is not a valid UF2 file (bad magic in block [expr {$o / 512}])"
		}
		if {$flags & 0x1} continue                                    ;# not for flash
		if {($flags & 0x2000) && $family != $rp2040_family} continue  ;# other target
		if {$psize == 0 || $psize > 476} {
			error "$filename has an invalid payload size in block [expr {$o / 512}]"
		}
		if {$addr < $flash_base} continue
		lappend blocks [list [expr {$addr - $flash_base}] \
			[string range $data [expr {$o + 32}] [expr {$o + 31 + $psize}]]]
	}
	if {[llength $blocks] == 0} {
		error "$filename does not contain RP2040 flash data"
	}

	set image ""
	set pos 0
	foreach block [lsort -integer -index 0 $blocks] {
		lassign $block addr payload
		if {$addr < $pos} {
			set payload [string range $payload [expr {$pos - $addr}] end]
			set addr $pos
		}
		if {$addr > $pos} {
			append image [string repeat "\xFF" [expr {$addr - $pos}]]
		}
		append image $payload
		set pos [expr {$addr + [string length $payload]}]
	}
	return $image
}

proc u32 {data pos} {
	binary scan $data "@${pos}iu" value
	return $value
}

proc u8 {data pos} {
	binary scan $data "@${pos}cu" value
	return $value
}

# Check whether the MultiROM menu + config area start at 'pos' in the flash
# image. The first config record always points to flash offset 0x8000 (right
# after the menu and config area) and must fit inside the image.
proc is_menu_at {flash len pos} {
	variable menu_size
	variable config_offset
	variable rom_name_max
	variable first_rom_offset
	variable mappers

	if {$pos + $menu_size > $len} {return 0}
	set init [expr {[u8 $flash [expr {$pos + 2}]] | ([u8 $flash [expr {$pos + 3}]] << 8)}]
	if {$init < 0x4000 || $init > 0xBFFF} {return 0}
	set rec [expr {$pos + $config_offset}]
	set first [u8 $flash $rec]
	if {$first < 0x20 || $first > 0x7E} {return 0}
	set mapper [u8 $flash [expr {$rec + $rom_name_max}]]
	if {![dict exists $mappers $mapper]} {return 0}
	set size   [u32 $flash [expr {$rec + $rom_name_max + 1}]]
	set offset [u32 $flash [expr {$rec + $rom_name_max + 5}]]
	if {$offset != $first_rom_offset || $size == 0} {return 0}
	return [expr {$pos + $offset + $size <= $len}]
}

# The firmware addresses its payload relative to __flash_binary_end, i.e. the
# end of the firmware binary. Its size varies between builds, so locate the
# menu ROM ("AB" header) followed by a valid config area.
proc locate_menu {flash} {
	variable search_limit
	set len [string length $flash]
	# Search a separate copy: string searches convert the value to a text
	# representation, which would slow down every later binary scan of $flash.
	set head [string range $flash 0 [expr {($len < $search_limit ? $len : $search_limit) - 1}]]
	set pos 0
	while {[set pos [string first "AB" $head $pos]] >= 0} {
		if {[is_menu_at $flash $len $pos]} {
			return $pos
		}
		incr pos
	}
	error "no PicoVerse 2040 MultiROM menu found in the UF2 image"
}

proc parse_records {flash} {
	variable config_offset
	variable record_size
	variable max_records
	variable rom_name_max

	set end_marker [string repeat "\xFF" $record_size]
	set result [list]
	set rec $config_offset
	for {set i 0} {$i < $max_records} {incr i} {
		if {[string range $flash $rec [expr {$rec + $record_size - 1}]] eq $end_marker} break
		set name [string range $flash $rec [expr {$rec + $rom_name_max - 1}]]
		set nul [string first "\x00" $name]
		if {$nul >= 0} {set name [string range $name 0 [expr {$nul - 1}]]}
		set name   [string trimright $name]
		set mapper [u8  $flash [expr {$rec + $rom_name_max}]]
		set size   [u32 $flash [expr {$rec + $rom_name_max + 1}]]
		set offset [u32 $flash [expr {$rec + $rom_name_max + 5}]]
		lappend result [list $name $mapper $size $offset]
		incr rec $record_size
	}
	return $result
}

proc mapper_info {mapper} {
	variable mappers
	if {[dict exists $mappers $mapper]} {
		return [dict get $mappers $mapper]
	}
	return [list "Unknown" "" 0 0]
}

# ---------------------------------------------------------------------------
# Cache files and generated extensions
# ---------------------------------------------------------------------------

proc persistent_dir {} {
	return [file join [file dirname $::env(OPENMSX_USER_DATA)] persistent picoverse2040]
}

proc clean_name {name} {
	set name [regsub -all {[^A-Za-z0-9 ._()+-]} $name _]
	set name [string trim $name " ."]
	if {$name eq ""} {set name "rom"}
	return $name
}

# Write a cache file unless an identical one already exists. openMSX may keep
# ROM files open while they are in use, so a file that cannot be replaced is
# written under a new name instead.
proc cache_file {name data} {
	variable cache_dir
	file mkdir $cache_dir
	set path [file join $cache_dir $name]
	set root [file rootname $path]
	set ext [file extension $path]
	for {set n 1} {$n < 100} {incr n} {
		if {[file exists $path] && [file size $path] == [string length $data]} {
			if {[read_binary $path] eq $data} {
				return $path
			}
		}
		if {![catch {write_binary $path $data}]} {
			return $path
		}
		set path "${root}_$n$ext"
	}
	error "unable to write cache file $path"
}

proc rom_data {index} {
	variable records
	variable image
	lassign [lindex $records $index] name mapper size offset
	lassign [mapper_info $mapper] desc romtype fixed bank
	set data [string range $image $offset [expr {$offset + $size - 1}]]
	if {$fixed > 0} {
		set data [string range $data 0 [expr {$fixed - 1}]]
		set pad [expr {$fixed - [string length $data]}]
	} elseif {$bank > 0} {
		set pad [expr {($bank - [string length $data] % $bank) % $bank}]
	} else {
		set pad 0
	}
	if {$pad > 0} {
		append data [string repeat "\xFF" $pad]
	}
	return $data
}

proc xml_escape {text} {
	string map {& &amp; < &lt; > &gt; \" &quot; ' &apos;} $text
}

proc default_hd_image {} {
	return [file join [persistent_dir] hd.dsk]
}

# USB drive image of the Nextor entries: the -hd option of the last insert,
# else setting picoverse2040_hd (read when the entry boots), else the default.
proc hd_path {} {
	variable hd_image
	if {$hd_image ne ""} {return $hd_image}
	if {$::picoverse2040_hd ne ""} {return [file normalize $::picoverse2040_hd]}
	return [default_hd_image]
}

# Generate the openMSX extension used for the embedded Nextor entries:
#   mode 10: Sunrise IDE (Nextor ROM + IDE) in the cartridge slot
#   mode 11: expanded cartridge slot, sub-slot 0 = Sunrise IDE, sub-slot 1 =
#            192KB memory mapper (same layout as loadrom_sunrise_mapper)
# openMSX resolves an extension name as share/extensions/<name>.xml, and lists
# every file there in its extension menus. The configuration is an internal
# detail of the PicoVerse cartridge, so it is written to the persistent folder
# instead and inserted through a relative name that leads there. Returns that
# name.
proc write_nextor_extension {with_mapper rom_file} {
	variable nextor_ext
	set ext_name [expr {$with_mapper ? "${nextor_ext}_Mapper" : $nextor_ext}]
	set title [expr {$with_mapper ? "PicoVerse 2040 Nextor Sunrise IDE + 192KB Mapper" : "PicoVerse 2040 Nextor Sunrise IDE"}]
	set hd [hd_path]
	file mkdir [file dirname $hd]

	set ide "<SunriseIDE id=\"PicoVerse 2040 Sunrise IDE\">
          <mem base=\"0x0000\" size=\"0x10000\"/>
          <rom>
            <filename>[xml_escape $rom_file]</filename>
          </rom>
          <master>
            <type>IDEHD</type>
            <filename>[xml_escape $hd]</filename>
            <size>100</size>
            <name>PicoVerse USB drive</name>
          </master>
        </SunriseIDE>"

	if {$with_mapper} {
		set slots "    <primary slot=\"any\">
      <secondary slot=\"0\">
        $ide
      </secondary>
      <secondary slot=\"1\">
        <MemoryMapper id=\"PicoVerse 2040 192KB Mapper\">
          <mem base=\"0x0000\" size=\"0x10000\"/>
          <size>192</size>
        </MemoryMapper>
      </secondary>
      <secondary slot=\"2\"/>
      <secondary slot=\"3\"/>
    </primary>"
	} else {
		set slots "    <primary slot=\"any\">
      <secondary slot=\"any\">
        $ide
      </secondary>
    </primary>"
	}

	set xml "<?xml version=\"1.0\" ?>
<!DOCTYPE msxconfig SYSTEM 'msxconfig2.dtd'>
<!-- Generated by picoverse2040.tcl, do not edit: it is rewritten on use. -->
<msxconfig>
  <info>
    <name>$title</name>
    <manufacturer>The Retro Hacker</manufacturer>
    <code/>
    <release_year>2026</release_year>
    <description>Used internally by picoverse2040.tcl for the Nextor entries of a PicoVerse 2040 UF2.</description>
    <type>external hard disk</type>
  </info>
  <devices>
$slots
  </devices>
</msxconfig>
"
	set dir [file join [persistent_dir] extensions]
	file mkdir $dir
	set path [file join $dir $ext_name.xml]
	set old ""
	if {[file exists $path]} {catch {set old [read_binary $path]}}
	if {$old ne $xml} {
		write_binary $path $xml
	}
	return [nextor_ext_ref $ext_name]
}

# Name that makes openMSX load <persistent>/picoverse2040/extensions/<name>.xml:
# extension names are resolved relative to <user dir>/share/extensions.
proc nextor_ext_ref {ext_name} {
	return "../../persistent/picoverse2040/extensions/$ext_name"
}

# Earlier versions wrote the Nextor configurations to share/extensions, where
# openMSX lists them as extensions. Remove those generated copies.
proc remove_legacy_extensions {} {
	variable nextor_ext
	foreach dir [glob -nocomplain -types d -directory [file join $::env(OPENMSX_USER_DATA) extensions] "${nextor_ext}*"] {
		set path [file join $dir hardwareconfig.xml]
		if {[catch {set xml [read_binary $path]}]} continue
		if {[string first "Generated by picoverse2040.tcl" $xml] >= 0} {
			catch {file delete -force $dir}
		}
	}
}

# ---------------------------------------------------------------------------
# Cartridge slot handling
# ---------------------------------------------------------------------------

proc resolve_slot {} {
	variable slot
	variable slot_ps
	variable slot_ss
	set info [machine_info external_slot slot$slot]
	set slot_ps [lindex $info 0]
	set slot_ss [lindex $info 1]
}

proc remove_current {} {
	variable current
	variable slot
	lassign $current kind what
	switch -- $kind {
		cart {catch {cart$slot eject}}
		ext  {catch {remove_extension $what}}
	}
	set current ""
}

proc remove_watchpoint {} {
	variable wp_id
	if {$wp_id ne ""} {
		catch {debug remove_watchpoint $wp_id}
		set wp_id ""
	}
}

proc install_watchpoint {} {
	variable wp_id
	variable monitor_addr
	remove_watchpoint
	set wp_id [debug set_watchpoint write_mem $monitor_addr \
		{[picoverse2040::cartridge_in_page2]} {picoverse2040::on_menu_write}]
}

# Watchpoint condition: only writes that reach the PicoVerse cartridge count,
# i.e. page 2 (0x8000-0xBFFF) must have the cartridge slot selected.
proc cartridge_in_page2 {} {
	variable slot_ps
	variable slot_ss
	set ps [expr {([debug read "ioports" 0xA8] >> 4) & 3}]
	if {$ps != $slot_ps} {return 0}
	if {$slot_ss eq "X"} {return 1}
	set ss_reg [debug read "slotted memory" [expr {0x40000 * $ps + 0xFFFF}]]
	expr {((($ss_reg ^ 255) >> 4) & 3) == $slot_ss}
}

proc on_menu_write {} {
	variable records
	variable pending
	variable state
	set index $::wp_last_value
	if {$state ne "menu" || $pending || $index >= [llength $records]} return
	# The menu follows the write with RST 00h; swap the cartridge outside of
	# the CPU emulation loop, then reset (the MSX is rebooting anyway).
	set pending 1
	after realtime 0 [list picoverse2040::boot_entry $index]
}

proc insert_menu {} {
	variable image
	variable menu_size
	variable menu_file
	variable slot
	variable current
	variable state
	variable selected
	variable pending

	remove_watchpoint
	remove_current
	resolve_slot
	set menu_file [cache_file "menu.rom" [string range $image 0 [expr {$menu_size - 1}]]]
	cart$slot insert $menu_file -romtype Page12
	set current [list cart $menu_file]
	install_watchpoint
	set state "menu"
	set selected -1
	set pending 0
}

proc boot_entry {index} {
	variable records
	variable slot
	variable current
	variable state
	variable selected
	variable pending
	variable own_boot

	set pending 0
	lassign [lindex $records $index] name mapper size offset
	lassign [mapper_info $mapper] desc romtype
	if {$romtype eq ""} {
		error "entry [expr {$index + 1}] ($name) uses unsupported mapper code $mapper"
	}

	remove_watchpoint
	remove_current
	set file [cache_file [format "%03d_%s.rom" [expr {$index + 1}] [clean_name $name]] [rom_data $index]]
	switch -- $romtype {
		nextor - nextor_map {
			set ext_name [write_nextor_extension [expr {$romtype eq "nextor_map"}] $file]
			set current [list ext [ext$slot $ext_name]]
		}
		default {
			cart$slot insert $file -romtype $romtype
			set current [list cart $file]
		}
	}
	set state "rom"
	set selected $index
	catch {message "PicoVerse 2040: $name ($desc)" info}
	# Tell on_boot that this reset starts the entry (a powered-off MSX does
	# not reset, so no boot event would clear the flag).
	set own_boot [expr {$::power ? 1 : 0}]
	reset
	return "Booting entry [expr {$index + 1}]: $name ($desc)"
}

# Powering the MSX off brings back the menu.
proc on_power_change {args} {
	variable state
	variable own_boot
	if {!$::power} {
		set own_boot 0
		if {$state eq "rom"} {
			after realtime 0 picoverse2040::restore_menu
		}
	}
}

# Any other reset of the MSX (reset command, menu or hotkey) also brings back
# the menu. Only the reset issued by boot_entry keeps the selected entry.
proc on_boot {} {
	variable state
	variable own_boot
	after boot picoverse2040::on_boot
	if {$own_boot} {
		set own_boot 0
		return
	}
	if {$state eq "rom" && ![replaying]} {
		after realtime 0 picoverse2040::restore_menu
	}
}

# Replaying a reverse/replay history re-executes the recorded resets and
# cartridge changes; don't interfere with it.
proc replaying {} {
	expr {![catch {dict get [reverse status] status} status] && $status eq "replaying"}
}

proc restore_menu {} {
	variable state
	variable image
	variable uf2_file
	if {$state ne "rom"} return
	# An entry restored from an earlier session: load the last UF2 first.
	if {$image eq "" && [catch {
		if {$::picoverse2040_uf2 eq ""} {error "no UF2 selected, use 'picoverse2040 insert <file.uf2>'"}
		load_uf2 $::picoverse2040_uf2
	} err]} {
		notify $err error
		return
	}
	if {[catch {insert_menu} err]} {
		notify $err error
		return
	}
	set ::picoverse2040_uf2 $uf2_file
	if {$::power} reset
}

# Track the cartridge across machine switches and savestate loads: re-arm the
# menu watchpoint when the new machine still holds our menu ROM, and keep an
# entry restored by a savestate running (the next reset brings the menu back).
proc on_machine_switch {} {
	variable state
	variable current
	variable wp_id
	variable slot
	variable menu_file
	variable pending
	variable own_boot
	variable machine_id

	after machine_switch picoverse2040::on_machine_switch
	set wp_id ""
	set pending 0
	set own_boot 0
	set state "idle"
	set current ""
	catch {set machine_id [machine]}
	if {[catch {set slots [machine_info external_slot]}]} return
	foreach s $slots {
		set cart [lindex [machine_info external_slot $s] 2]
		set letter [string range $s 4 end]
		if {$cart ne "" && $cart eq $menu_file} {
			set slot $letter
			if {[catch {resolve_slot; install_watchpoint}]} return
			set current [list cart $cart]
			set state "menu"
			return
		}
		if {[is_nextor_ext $cart] || ([is_cached_rom $cart] && ![is_menu_rom $cart])} {
			set slot $letter
			catch resolve_slot
			set current [list [expr {[is_nextor_ext $cart] ? "ext" : "cart"}] $cart]
			set state "rom"
			return
		}
	}
	# Extensions restored from a savestate or setup are not always reported
	# in their cartridge slot.
	set ext [restored_nextor_ext]
	if {$ext ne ""} {
		if {$slot eq ""} {set slot $::picoverse2040_slot}
		catch resolve_slot
		set current [list ext $ext]
		set state "rom"
	}
}

proc restored_nextor_ext {} {
	foreach ext [list_extensions] {
		if {[is_nextor_ext $ext]} {return $ext}
	}
	return ""
}

proc is_menu_rom {file} {
	regexp {^menu(_[0-9]+)?\.rom$} [file tail $file]
}

proc is_nextor_ext {name} {
	variable nextor_ext
	string match "${nextor_ext}*" [file tail $name]
}

# ROM files extracted by this script (menu or entries) from any UF2.
proc is_cached_rom {file} {
	set root "[file join [persistent_dir] cache]/"
	string equal -nocase -length [string length $root] $root $file
}

# ---------------------------------------------------------------------------
# User command
# ---------------------------------------------------------------------------

proc load_uf2 {filename} {
	variable image
	variable records
	variable uf2_file
	variable cache_dir

	set given $filename
	set filename [file normalize $filename]
	if {![file exists $filename]} {
		set msg "file not found: $filename"
		# Tcl treats backslashes as escapes: C:\temp\x.uf2 arrives as "C:<TAB>empx.uf2".
		if {![string match {*[/\\]*} $given] || [regexp {[\x00-\x1f]} $given]} {
			append msg "\nIn the openMSX console backslashes are escape characters:\
				use forward slashes (C:/temp/multirom.uf2) or braces ({C:\\temp\\multirom.uf2})."
		}
		error $msg
	}
	set flash [uf2_to_flash $filename]
	set pos [locate_menu $flash]
	set new_image [string range $flash $pos end]
	set new_records [parse_records $new_image]
	if {[llength $new_records] == 0} {
		error "the UF2 MultiROM configuration area is empty"
	}
	set len [string length $new_image]
	foreach rec $new_records {
		lassign $rec name mapper size offset
		if {$offset + $size > $len} {
			error "entry '$name' points outside of the UF2 image"
		}
	}

	set key [format %x_%x [file size $filename] [file mtime $filename]]
	set base [clean_name [file rootname [file tail $filename]]]
	set new_cache_dir [file join [persistent_dir] cache "${base}_$key"]
	# Drop caches of older builds of the same UF2 (files still in use stay).
	foreach dir [glob -nocomplain -types d -directory [file join [persistent_dir] cache] "${base}_*"] {
		set suffix [string range [file tail $dir] [string length "${base}_"] end]
		if {$dir ne $new_cache_dir && [regexp {^[0-9a-f]+_[0-9a-f]+$} $suffix]} {
			catch {file delete -force $dir}
		}
	}
	set image $new_image
	set records $new_records
	set uf2_file $filename
	set cache_dir $new_cache_dir
}

proc parse_options {arglist} {
	variable slot
	variable hd_image
	set file ""
	set new_slot $::picoverse2040_slot
	set new_hd   ""
	for {set i 0} {$i < [llength $arglist]} {incr i} {
		set arg [lindex $arglist $i]
		switch -- $arg {
			-slot {
				set new_slot [string tolower [lindex $arglist [incr i]]]
			}
			-hd {
				set new_hd [lindex $arglist [incr i]]
			}
			default {
				if {$file ne ""} {error "unexpected argument: $arg"}
				set file $arg
			}
		}
	}
	if {[string length $new_slot] != 1 || [info commands ::cart$new_slot] eq ""} {
		error "invalid cartridge slot '$new_slot' (available: [string map {slot {}} [machine_info external_slot]])"
	}
	set slot $new_slot
	set hd_image [expr {$new_hd ne "" ? [file normalize $new_hd] : ""}]
	return $file
}

proc cmd_insert {arglist} {
	variable uf2_file
	variable state
	variable slot
	variable records

	set old_slot $slot
	set file [parse_options $arglist]
	if {$file eq ""} {
		set file [expr {$uf2_file ne "" ? $uf2_file : $::picoverse2040_uf2}]
		if {$file eq ""} {error "no UF2 file given"}
	}
	if {$state ne "idle" && $old_slot ne "" && $old_slot ne $slot} {
		set new_slot $slot
		set slot $old_slot
		remove_watchpoint
		remove_current
		set slot $new_slot
		set state "idle"
	}
	load_uf2 $file
	insert_menu
	set ::picoverse2040_uf2 $uf2_file
	reset
	return "PicoVerse 2040 inserted in slot $slot: [file tail $uf2_file] ([llength $records] entries)"
}

proc cmd_list {} {
	variable records
	variable selected
	if {[llength $records] == 0} {error "no UF2 loaded, use 'picoverse2040 insert <file.uf2>'"}
	set result ""
	set i 0
	foreach rec $records {
		lassign $rec name mapper size offset
		lassign [mapper_info $mapper] desc
		set mark [expr {$i == $selected ? "*" : " "}]
		append result [format "%s%3d  %-50s %-6s %8d\n" $mark [incr i] $name $desc $size]
	}
	return $result
}

proc cmd_info {} {
	variable uf2_file
	variable state
	variable slot
	variable records
	variable selected
	variable cache_dir
	if {$uf2_file eq ""} {return "No PicoVerse 2040 UF2 loaded."}
	set result "UF2 file : $uf2_file\n"
	append result "Slot     : $slot\n"
	append result "Entries  : [llength $records]\n"
	append result "State    : $state"
	if {$state eq "rom" && $selected >= 0} {
		append result " ([lindex $records $selected 0])"
	}
	append result "\nHD image : [hd_path]"
	append result "\nCache    : $cache_dir"
	return $result
}

proc cmd_eject {} {
	variable state
	remove_watchpoint
	remove_current
	set state "idle"
	return "PicoVerse 2040 removed."
}

set_help_text picoverse2040 \
"Run a PicoVerse 2040 MultiROM UF2 image in openMSX (script $version).

picoverse2040 insert <file.uf2> \[-slot <a|b>\] \[-hd <image>\]
    Insert the PicoVerse 2040 cartridge built into the UF2 and reset the MSX.
    The MSX boots the PicoVerse menu with all the entries of the UF2.
    -slot  cartridge slot to use (default: setting picoverse2040_slot)
    -hd    hard disk image used as USB drive by the Nextor entries until the
           next insert (default: setting picoverse2040_hd, read when a
           Nextor entry boots, or picoverse2040/hd.dsk)
picoverse2040 insert
    Re-insert the last UF2 (setting picoverse2040_uf2).
picoverse2040 menu
    Return to the menu (same as resetting the MSX).
picoverse2040 boot <n>
    Skip the menu and boot entry <n> (see 'picoverse2040 list').
picoverse2040 list
    List the entries of the loaded UF2 (* marks the running one).
picoverse2040 info
    Show the current status.
picoverse2040 eject
    Remove the PicoVerse 2040 cartridge.

Resetting the MSX, turning the power off and on, or restarting openMSX with
an entry still in the slot brings back the menu.

Instead of this command you can also insert the 'PicoVerse 2040 MultiROM'
extension (it loads the UF2 in setting picoverse2040_uf2, the last UF2 used),
or select the .uf2 file as ROM image of a cartridge slot.

In this console backslashes are escape characters: write Windows paths with
forward slashes (C:/temp/multirom.uf2) or between braces ({C:\\temp\\multirom.uf2})."

proc picoverse2040 {args} {
	variable records
	variable uf2_file
	set sub [lindex $args 0]
	set rest [lrange $args 1 end]
	switch -- $sub {
		insert  {return [cmd_insert $rest]}
		eject   {return [cmd_eject]}
		list    {return [cmd_list]}
		info    -
		""      {return [cmd_info]}
		menu {
			if {$uf2_file eq ""} {error "no UF2 loaded, use 'picoverse2040 insert <file.uf2>'"}
			insert_menu
			reset
			return "PicoVerse 2040 menu restored."
		}
		boot {
			if {$uf2_file eq ""} {error "no UF2 loaded, use 'picoverse2040 insert <file.uf2>'"}
			set n [lindex $rest 0]
			if {![string is integer -strict $n] || $n < 1 || $n > [llength $records]} {
				error "entry number must be between 1 and [llength $records]"
			}
			return [boot_entry [expr {$n - 1}]]
		}
		default {
			if {[file isfile $sub]} {return [cmd_insert $args]}
			error "unknown subcommand '$sub', see 'help picoverse2040'"
		}
	}
}

proc tab_completion {args} {
	variable records
	set sub [lindex $args 1]
	set prev [lindex $args end-1]
	if {[llength $args] == 2} {
		return [list insert eject menu boot list info]
	}
	switch -- $sub {
		insert {
			if {$prev eq "-slot"} {
				return [string map {slot {}} [machine_info external_slot]]
			}
			return [concat [list -slot -hd] [utils::file_completion {*}$args]]
		}
		boot {
			set result [list]
			for {set i 1} {$i <= [llength $records]} {incr i} {lappend result $i}
			return $result
		}
	}
	return [list]
}

set_tabcompletion_proc picoverse2040 [namespace code tab_completion]

# ---------------------------------------------------------------------------
# Slot watcher: openMSX has no Tcl event for cartridge insertion, so the
# external slots are polled to catch the PicoVerse extension or a .uf2 file
# inserted from the GUI, the command line or the console.
# ---------------------------------------------------------------------------

proc notify {text level} {
	catch {message "PicoVerse 2040: $text" $level}
}

# A .uf2 file was inserted as a ROM image: take over the slot.
proc take_over_uf2 {letter file} {
	catch {cart$letter eject}
	if {[catch {cmd_insert [list $file -slot $letter]} result]} {
		notify $result error
	} else {
		notify $result info
	}
}

# The PicoVerse 2040 extension was inserted, or the slot still holds media of
# an earlier session (a cached ROM or a generated Nextor extension, for example
# from a setup restored when openMSX starts): replace it with the menu of the
# last UF2. Without a UF2 the media stays; the placeholder shows how to select
# one.
proc take_over {letter kind inserted} {
	variable uf2_file
	variable notified
	set file [expr {$uf2_file ne "" ? $uf2_file : $::picoverse2040_uf2}]
	if {$file eq "" || ![file isfile $file]} {
		if {$notified ne $inserted} {
			set notified $inserted
			notify "no UF2 selected: choose the .uf2 file as ROM image in Media > Cartridge Slot, or type 'picoverse2040 insert <file.uf2>' in the console" warning
		}
		return
	}
	set notified ""
	switch -- $kind {
		ext  {catch {remove_extension $inserted}}
		cart {catch {cart$letter eject}}
	}
	if {[catch {cmd_insert [list $file -slot $letter]} result]} {
		notify $result error
	} else {
		notify $result info
	}
}

proc check_slots {} {
	variable ext_config
	variable state
	variable machine_id
	# After a savestate load, wait for on_machine_switch to classify the slots.
	set settled [expr {[machine] eq $machine_id}]
	foreach s [machine_info external_slot] {
		set inserted [lindex [machine_info external_slot $s] 2]
		if {$inserted eq ""} continue
		set letter [string range $s 4 end]
		if {[string match -nocase *.uf2 $inserted]} {
			take_over_uf2 $letter $inserted
			return
		}
		if {$inserted eq $ext_config || [string match "$ext_config (*)" $inserted]} {
			take_over $letter ext $inserted
			return
		}
		if {$state eq "idle" && $settled} {
			if {[is_nextor_ext $inserted]} {
				take_over $letter ext $inserted
				return
			}
			if {[is_cached_rom $inserted]} {
				take_over $letter cart $inserted
				return
			}
		}
	}
	# Extensions restored from a setup are not always reported in their
	# cartridge slot: look them up by name and use the configured slot.
	foreach ext [list_extensions] {
		if {$ext eq $ext_config || [string match "$ext_config (*)" $ext]} {
			take_over $::picoverse2040_slot ext $ext
			return
		}
	}
	if {$state eq "idle" && $settled} {
		set ext [restored_nextor_ext]
		if {$ext ne ""} {
			take_over $::picoverse2040_slot ext $ext
		}
	}
}

proc watch_slots {} {
	variable watch_interval
	after realtime $watch_interval picoverse2040::watch_slots
	catch {check_slots}
}

trace add variable ::power write [namespace code on_power_change]
catch remove_legacy_extensions
catch {set machine_id [machine]}
after machine_switch picoverse2040::on_machine_switch
after boot picoverse2040::on_boot
after realtime 0 picoverse2040::watch_slots

namespace export picoverse2040

} ;# namespace picoverse2040

namespace import picoverse2040::*
