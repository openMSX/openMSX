# $Id$

include build/node-start.mk

DIST:=*.m4

# Backwards compatibility for auto* system:
DIST+=Makefile.am

include build/node-end.mk

