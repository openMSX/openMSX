# $Id$

include build/node-start.mk

DIST:= \
	config.guess detectsys.sh tcl-search.sh install-recursive.sh \
	main.mk node-end.mk node-start.mk \
	info2code.mk components.mk probe.mk probe-results.mk \
	version.mk custom.mk \
	flavour-*.mk cpu-*.mk platform-*.mk

include build/node-end.mk

