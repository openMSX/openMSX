# Customize openMSX to your system/preferences.

# Directory to install to.
# openMSX is always installed into a single self-contained directory.
# But you can change that directory to for example /usr/local/openMSX
# or /usr/games/openMSX if you like.
INSTALL_BASE:=/opt/openMSX

# Add revision number to executable file name? This applies only to
# development versions, not to release versions (see version.py).
VERSION_EXEC:=false

# Create a symbolic link to the installed binary?
# This link is placed in a location that is typically in a user's path:
# /usr/local/bin for system-wide installs and ~/bin for personal installs.
# This setting is only relevant on systems that support symbolic links.
SYMLINK_FOR_BINARY:=true

# Install content of Contrib/ directory?
# Currently this contains a version of C-BIOS.
INSTALL_CONTRIB:=true
