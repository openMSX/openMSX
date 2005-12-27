# $Id$
#
# Configuration for FreeBSD 4.x.

# Default compiler.
#  Default C++ compiler of FreeBSD-4 is g++ 2.9x.
#  openMSX requires g++3 or higher.
#  Here we expect the user (builder) has installed gcc33 via ports/packages.
OPENMSX_CXX?=g++33

include build/platform-freebsd.mk
