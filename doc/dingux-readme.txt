openMSX for Dingux prerelease
=============================

This is an openMSX development version modified and compiled for running on
Dingux. It is a very early port, so expect it to have a lot of rough edges.
For example, startup and save state loading is slow and key bindings are not
sufficient for some games.

I'm making this early version public so people can play with it and give
feedback. Please help me make this a great way to play MSX games on a handheld.
Contact info is at the bottom of this text.

About Dingux
------------

Dingux is Linux for the Dingoo A-320 and possibly other similar devices in the
future.

About the Dingoo A-320:
  http://en.wikipedia.org/wiki/Dingoo

Dingux blog:
  http://www.dingux.com/

Dingux downloads:
  http://code.google.com/p/dingoo-linux/downloads/list

Installing Dingux
-----------------

First, download the dual boot installer and follow the instructions in its
README file.

Next, download the Dingux system and copy it to your miniSD card as described
in its README file. New Dingux system releases are made regularly and I will
only test openMSX against the latest Dingux system release, so please keep your
system updated.

Installing openMSX
------------------

Copy the directory named "local" from the openMSX for Dingux ZIP file to the
root of the miniSD card. If you have run Dingux from the minSD card before,
there will already be a directory named "local". You can safely merge both
directories.

openMSX comes with the C-BIOS system ROMs, which is an open source MSX BIOS.
If you'd like to use other system ROMs, for example to get disk support or
because you want to emulate the exact MSX model you had years ago, you have
to install those yourself. Please read this for more info:
  http://openmsx.sourceforge.net/manual/setup.html#systemroms

Using openMSX
-------------

These are the key mappings:

Dingoo key:         MSX key:            OSD action:
D-pad up            Cursor up           Cursor up
D-pad down          Cursor down         Cursor down
D-pad left          Cursor left         Value - / Previous page
D-pad right         Cursor right        Value + / Next page
A button            CTRL
B button            Graph
X button            Space
Y button            Shift
Left shoulder       Tab
Right shoulder                          Toggle OSD menu
START button        Enter               Confirm
SELECT button       Escape              Cancel

This is just the default openMSX key map for PCs that happens to work (mostly)
for Dingux too. If you have an idea for a better key mapping, please let me
know. Ideally, the key mapping should work for as many games as possible.

Customizing openMSX
-------------------

openMSX can be configured using the settings.xml file. This file only stores
settings that have a non-default value.

Things like key bindings can be customized by writing short Tcl scripts.
Please have a look in the share/openmsx/scripts directory for examples. Any
script you place there will be executed by openMSX on startup.

Please read this document to learn about the commands and settings of openMSX:
  http://openmsx.sourceforge.net/manual/commands.html

Building openMSX
----------------

If you want to make bigger changes, you have to compile your own openMSX for
Dingux.

The first step is to download the Dingux toolchain. This toolchain is for x86
Linux; if you have an x86 machine but no Linux, you can install Linux in a
virtual machine. Extract the toolchain to /opt; anywhere else won't work.

Now get the openMSX sources from Subversion:
  svn co https://openmsx.svn.sourceforge.net/svnroot/openmsx/openmsx/trunk

Download the Dingux-specific patch from the openMSX site and apply it:
  patch -p0 < openmsx-dingux-r12345.diff

Now you can start the build:
  chmod a+x dingux-build.sh
   ./dingux-build.sh

After the build is done, you can find the executable in:
  derived/mipsel-linux-opt-3rd/bin

More information about building openMSX:
  http://openmsx.sourceforge.net/manual/compile.html

Feedback
--------

If you found a bug, made a patch, created a cool theme or have anything else
you'd like to share, please mail me:
  Maarten ter Huurne <maarten@treewalker.org>
