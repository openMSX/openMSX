# $Id$

include build/node-start.mk

DIST:= \
	*.html *.css

# Backwards compatibility for auto* system:
DIST+=Makefile.am

include build/node-end.mk

