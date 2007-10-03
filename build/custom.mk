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
# This setting is only relevant on systems that support symbolic links.
SYMLINK_FOR_BINARY:=true

# Install content of Contrib/ directory?
# Currently this contains a version of C-BIOS.
INSTALL_CONTRIB:=true

# Forced disabling of libraries: these libraries will not be used, even if they
# are present on the system and enabled for the platform+flavour you are using.
# This works for every library openMSX uses, but disabling a mandatory library
# will prevent openMSX from compiling.
# Useful values are GL, GLEW and JACK.
DISABLED_LIBRARIES:=GL GLEW JACK
