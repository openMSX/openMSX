# $Id$

include build/node-start.mk

SUBDIRS:= \
	3rdparty package-darwin package-slackware

DIST:= \
	tcl-search.sh \
	main.mk node-end.mk node-start.mk entry.mk \
	custom.mk 3rdparty.mk \
	flavour-*.mk platform-*.mk \
	*.py

include build/node-end.mk
