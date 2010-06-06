openMSX for Dingux
==================

This text is a quick guide for the Dingoo port of openMSX. It is still a bit of
an early port, so expect it to have some rough edges. For example, startup is
slow (expect about 10 seconds). Especially important is to not add too many and
especially too big system roms. This makes start up and loading states (the
first time) loads slower.

We are releasing it so people can play with it and give us feedback. Please
help us make this a great way to play MSX games on a handheld.
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

New Dingux site hub:
  http://dingoonity.org/

Installing Dingux
-----------------

First, download the dual boot installer and follow the instructions in its
README file.

Next, download the Dingux system and copy it to your miniSD card as described
in its README file. New Dingux system releases are made regularly and we will
only test openMSX against the latest Dingux system release, so please keep your
system updated.

Installing openMSX
------------------

This info is also in the openMSX Compilation Guide:
  http://openmsx.sourceforge.net/manual/compile.html#installstandalone

Copy the directory named "local" from the openMSX for Dingux ZIP file to the
root of the miniSD card. If you have run Dingux from the miniSD card before,
there will already be a directory named "local". You can safely merge both
directories.

openMSX comes with the C-BIOS system ROMs, which is an open source MSX BIOS.
If you'd like to use other system ROMs, for example to get disk support or
because you want to emulate the exact MSX model you had years ago, you have
to install those yourself. Please read this for more info:
  http://openmsx.sourceforge.net/manual/setup.html#systemroms
As mentioned before, currently it's wise to include only those system ROMs you
really need. Better not install large ones either (e.g. the MoonSound ROM),
unless you don't care about very long start up times.

Using openMSX
-------------

The keymappings are documented in the openMSX User's Manual:
  http://openmsx.sourceforge.net/manual/user.html#keymapping


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

This is described in the Compilation Guide (with specific instructions for
Dingux):
  http://openmsx.sourceforge.net/manual/compile.html

Feedback
--------

If you found a bug, made a patch, created a cool theme or have anything else
you'd like to share, please contact us, see
  http://openmsx.sourceforge.net/manual/user.html#contact
