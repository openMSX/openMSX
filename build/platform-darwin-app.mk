# $Id$
#
# Configuration for creating a Darwin app folder.
# In practice, this is used for Mac OS X; I'm not sure all of it applies to
# other Darwin-based systems.

include build/platform-darwin.mk

# Enable prebinding for better startup performance.
# On OS X 10.3 the system frameworks are using overlapping addresses, so
# prebinding fails. Let's try again later.
#LINK_FLAGS+=-prebind

# The app folder will set a hi-res icon, so the openMSX process should not
# replace this with its own low-res icon.
SET_WINDOW_ICON:=false
