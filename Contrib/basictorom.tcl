### basictorom.tcl ###
#
# This script was developed together with Daniel Vik to have an automated tool
# to convert BASIC programs to ROM files. See this forum thread for more
# details:
#
#   http://www.msx.org/forumtopic9249.html
#
# To use it, put a file 'prog.bas' in the current directory (can be either in
# ascii format or an already tokenized basic file). Then execute this script by
# using the openMSX commandline. And after a few seconds the ROM image
# 'prog.rom' will be generated.
#
#   input: prog.bas
#   output prog.rom
#   start with:  openmsx -script basictorom.tcl
#
# Note: This script only works on MSX machines that have a disk drive and have
#       MSX-BASIC built-in. So for example it won't work on the default C-bios
#       based machines. So either select a different MSX machine as your
#       default machine, or pass the '-machine <machine-config>' as extra
#       option when starting openMSX.
#

proc do_stuff1 {} {
  # insert openMSX ramdisk in the MSX disk drive
  diska ramdsk

  # import host file to ramdisk
  diskmanipulator import diska "prog.bas"

  # change basic start address
  poke16 0xf676 0x8011

  # add rom header
  poke   0x8000 0x41
  poke   0x8001 0x42
  poke16 0x8002 0x0000
  poke16 0x8004 0x0000
  poke16 0x8006 0x0000
  poke16 0x8008 0x8010
  poke16 0x800a 0x0000
  poke16 0x800c 0x0000
  poke16 0x800e 0x0000
  poke   0x8010 0x00

  # instruct MSX to load the BASIC program
  type "load\"prog.bas\"\r"

  # give MSX some time to process this
  #  wait long enough so that even very long BASIC programs can be loaded
  after time 100 do_stuff2
}

proc do_stuff2 {} {
  # save rom file
  set data [debug read_block "memory" 0x8000 0x4000]
  set file [open "prog.rom" "WRONLY CREAT TRUNC"]
  fconfigure $file -translation binary
  puts -nonewline $file $data
  close $file

  # exit emulator
  exit
}

# don't store the settings below for future openmsx sessions
set save_settings_on_exit false
# don't show MSX screen (remove if you want to see what's going on)
set renderer none
# go as fast as possible
set throttle off

# give emulated MSX some time to boot
after time 20 do_stuff1
