# Configuration for Maemo 5 devices.

# Maemo is based on Linux.
include build/platform-linux.mk

# Workaround for SDL bug 586: the Matchbox2 window manager will not give input
# events to our window if we set an icon, because SDL sets the wrong flags on
# the icon.
#   http://bugzilla.libsdl.org/show_bug.cgi?id=586
#   http://talk.maemo.org/showthread.php?t=31696
# This bug is fixed in SDL 1.2.14 (at least, I verified that the patch has been
# applied), but Maemo 5 is using an older SDL.
SET_WINDOW_ICON:=false

# Link to X11 lib, Needed for the code that configures window composition.
LINK_FLAGS+=-lX11
