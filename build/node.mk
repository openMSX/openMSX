# $Id$

include build/node-start.mk

DIST:= \
	config.guess detectsys.sh tcl-search.sh \
	configure.ac \
	main.mk node-end.mk node-start.mk \
	flavour-*.mk platform-*.mk

include build/node-end.mk

