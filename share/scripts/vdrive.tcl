# v-drive functionality of blueMSX implemented in a TCL script for openMSX
# version 1.0
#
# What does it do:
#   It is a simple TCL script that binded to a hotkey (f.i. F9+ALT) allows
#   users to swap disks without using the commandconsole or Compass for
#   file selection. This is especially usefull for games and demos that
#   span over multiple disks.
#
# Preparations:
# 1. If you have a software that spans multiple disks, you have to name then
#    according to the scheme: name+digit+extention for example [metal1.dsk,
#    metal2.dsk, metal3.dsk], ofcourse they may all be compressed with gzip
#    so that you end up with [metal1.dsk.gz, metal2.dsk.gz, metal3.dsk.gz].
#    The script recognizes as extentions dsk, di1, di2 and xsa, with an
#    optinal '.gz' affix.
#
# 2. bind the vdrive script to a hotkey for instance use in the console:
#      bind ALT+F9 "vdrive diska"
#      bind ALT+F10 "vdrive diskb"
#    using the vdrive script without a drive parameter will make
#    it use diska
#
# Using:
#   While emulating an MSX, and the sofware ask for the next disk simply
#   press the hot-key. The script will select the next disk in the sequence
#   if the last disk is reached, the script will select the first disk
#   again. So you actually are cycling through the entire disk set as shown
#   in the diagram below
#
#    _"disk1.dsk" => ALT+F9 => "disk2.dsk" => ALT+F9 => "disk3.dsk" _
#   |                                                                |
#    <= <= <= <= <= <= <= ALT+F9  <= <= <= <= <= <= <= <= <= <= <= <=
#
#
# credits:
#   original alt+f9 v-DRIVE idea by the blueMSX crew.
#   copyright 2005 david heremans all rights reserved
#


proc vdrive { { diskdrive "diska" } } { 
	#get current disk in diska
	set image ""
	set image [ $diskdrive ]
#	#if diska doesn't exist (invalid command name "diska") then quit
#	if [ string equal $image "" ] {
#	  error "no diska available"
#	}

	# check if there is a disk in diska
	if [ string equal [ string range $image 0 43 ] "There is currently no disk inserted in drive" ] {
	  error $image
	}

	#get imagename part
	set image [ string range $image 14 end-1 ]

	# skip if it is a 'special' disk
	if [ string equal "-ramdsk" $image ] {
	  error "vdrive not possible on ramdsk"
	}

	#skip if it is a DirAsDisk
	if [ file isdirectory $image ] {
	  error "vdrive not possible on DirAsDisk"
	}

	#remove (dsk|di1|di2|xsa)(.gz)? extention
	set ext ""
	set ext2 [ string range $image end-2 end ]
	if [ string equal -nocase ".gz" $ext2  ] {
		append ext $ext2
		set image [ string range $image 0 end-3 ]
	}

	set ext2 [ string range $image end-3 end ]
	foreach i { ".dsk" ".di1" ".di2" ".xsa" } {
		if [ string equal -nocase "$i" "$ext2"  ] {
			append ext2 $ext
			set ext $ext2
			set image [ string range $image 0 end-4 ]
		}
	}
	set digit [ string index $image end ]
	set image [ string range $image 0 end-1 ]

	if { ! [ string is digit $digit ] } {
	  error "no digit as last char in name"
	}
	# increase the number by 1 until file exists
	# file could be erased while testing so we can not assume that the
	# original filename will be found again!!
	set origdigit $digit
	set digit [ expr $digit + 1 ]
	set test ""
	append test $image $digit $ext
	while { $digit != $origdigit && ! [ file exists $test ] } {
		set digit [ expr $digit + 1 ]
		if { $digit == 10 } { set digit 0 }
		set test ""
		append test $image $digit $ext
	}

	if { $digit != $origdigit && [ file exists $test ] } {
		puts " New diskimage $test"
		diska $test
	}

}
