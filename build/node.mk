# $Id$

include build/node-start.mk

SUBDIRS:=\
	m4

DIST:= \
	config.guess detectsys.sh \
	configure.ac \
	main.mk node-end.mk node-start.mk \
	flavour-*.mk platform-*.mk

include build/node-end.mk

