# $Id$
#
# Customize openMSX to your system/preferences.

# Directory to install to.
# openMSX is always installed into a single self-contained directory.
# But you can change that directory to for example /usr/local/openMSX
# or /usr/games/openMSX if you like.
INSTALL_BASE:=/opt/openMSX

# Add ChangeLog version number to executable file name? This applies only to
# development versions, not to release versions (see version.mk).
VERSION_EXEC:=false

# Create a symbolic link to the installed binary?
# This link is placed in a location that is typically in a user's path:
# /usr/local/bin for system-wide installs and ~/bin for personal installs.
SYMLINK_FOR_BINARY:=true

# do you want to use ccache to possibly speed up subsequent builds?
USE_CCACHE:=false
