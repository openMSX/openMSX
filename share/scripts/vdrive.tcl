set_help_text vdrive \
{v-drive functionality of blueMSX implemented in a Tcl script for openMSX
version 1.0

What does it do:
  It is a simple Tcl script that bound to a hotkey (f.i. F9+ALT) allows
  users to swap disks without using the commandconsole or Catapult for
  file selection. This is especially useful for games and demos that
  span over multiple disks.

Preparations:
1. If you have software that spans multiple disks, you have to name them
   according to the scheme: name+digit+extension for example [metal1.dsk,
   metal2.dsk, metal3.dsk]
   Of course they may all be compressed with gzip, so you'll end up with
   [metal1.dsk.gz, metal2.dsk.gz, metal3.dsk.gz].
   The script recognizes 'dsk', 'di1', 'di2', 'xsa' and 'dmk' extensions,
   with an optional '.gz' suffix.

2. bind the vdrive script to a hotkey, for instance type in the console:
     bind ALT+F9 "vdrive diska"
     bind ALT+F10 "vdrive diskb"
   vdrive will default to 'diska' when no drive parameter is specified.
   Note: the two bind commands above are already the default key bindings;
         you only need to execute them if you want a different key binding.

Using:
  While emulating an MSX, and the sofware asks for the next disk, simply
  press the hot-key. The script will select the next disk in the sequence.
  If the last disk is reached, the script will select the first disk
  again. So you actually are cycling through the entire disk set as shown
  in the diagram below

   _"disk1.dsk" => ALT+F9 => "disk2.dsk" => ALT+F9 => "disk3.dsk" _
  |                                                                |
   <= <= <= <= <= <= <= ALT+F9  <= <= <= <= <= <= <= <= <= <= <= <=


credits:
  original alt+f9 v-DRIVE idea by the blueMSX crew.
  copyright 2005 David Heremans
}

bind_default ALT+F9  "vdrive diska"
bind_default ALT+F10 "vdrive diskb"

proc vdrive {{diskdrive "diska"}} {
	# get current disk
	if {[catch {set cmd [$diskdrive]}]} {error "No such drive: $diskdrive"}

	# skip for empty drive or 'special' disk
	set options [lindex $cmd 2]
	if       {"empty" in $options} {
		error "No disk in drive: $diskdrive"
	} elseif {"ramdsk" in $options} {
		error "Vdrive not possible on ramdsk"
	} elseif {"dirasdisk" in $options} {
		error "Vdrive not possible on DirAsDisk"
	}

	#remove (dsk|di1|di2|xsa|dmk)(.gz)? extention
	set image [lindex $cmd 1]
	set ext ""

	set ext2 [file extension $image]
	foreach i [list ".gz" ".zip"] {
		if {[string equal -nocase $i $ext2]} {
			set ext $ext2
			set image [file rootname $image]
			break
		}
	}

	set ext2 [file extension $image]
	foreach i {".dsk" ".di1" ".di2" ".xsa" ".dmk"} {
		if {[string equal -nocase $i $ext2]} {
			set ext ${ext2}${ext}
			set image [file rootname $image]
			break
		}
	}

	set digit [string index $image end]
	set image [string range $image 0 end-1]

	if {![string is digit $digit]} {
		error "No digit as last char in name"
	}

	# increase the number by 1 until file exists
	# file could be erased while testing so we can not assume that the
	# original filename will be found again!!
	set origdigit $digit
	while {true} {
		incr digit
		if {$digit == 10} {set digit 0}
		set test ${image}${digit}${ext}
		if {[file exists $test]} {
			diska $test
			return "New diskimage: $test"
		}
		if {$digit == $origdigit} {return ""}
	}
}
