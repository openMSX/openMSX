# $Id$

include build/node-start.mk

DIST:= \
	README \
	README.cbios cbios \
	README.openmsx-control \
	openmsx-control-stdio.cc openmsx-control-socket.cc

include build/node-end.mk

