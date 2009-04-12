# $Id$

include build/node-start.mk

SUBDIRS:= \
	3rdparty package-darwin package-slackware

DIST:= \
	tcl-search.sh \
	main.mk node-end.mk node-start.mk entry.mk \
	probe_defs.mk \
	custom.mk 3rdparty.mk bindist.mk \
	flavour-*.mk cpu-*.mk platform-*.mk \
	*.py

include build/node-end.mk
