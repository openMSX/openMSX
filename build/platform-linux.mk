# $Id$
#
# Configuration for Linux machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# File name extension of executables.
EXEEXT:=

COMPILE_FLAGS+=-pthread
LINK_FLAGS+=-pthread
