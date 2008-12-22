# $Id$

include build/node-start.mk

DIST:= \
	README \
	README.cbios cbios \
	README.openmsx-control \
	openmsx-control-stdio.cc openmsx-control-socket.cc \
	basictorom.tcl

include build/node-end.mk

