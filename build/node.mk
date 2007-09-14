# $Id$

include build/node-start.mk

SUBDIRS:= \
	3rdparty package-darwin package-slackware

DIST:= \
	config.guess detectsys.sh tcl-search.sh install-recursive.sh \
	main.mk node-end.mk node-start.mk entry.mk \
	info2code.mk components.mk probe.mk probe-results.mk \
	version.mk custom.mk 3rdparty.mk bindist.mk \
	flavour-*.mk cpu-*.mk platform-*.mk

include build/node-end.mk
