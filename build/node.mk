# $Id$

include build/node-start.mk

DIST:= \
	config.guess detectsys.sh tcl-search.sh \
	main.mk node-end.mk node-start.mk \
	info2code.mk probe.mk \
	flavour-*.mk platform-*.mk

include build/node-end.mk

