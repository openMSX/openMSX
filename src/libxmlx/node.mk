# $Id$

include build/node-start.mk

SRC_ONLY:=libxmlx
HDR_ONLY:=xmlx

DIST:= \
	AUTHORS COPYING ChangeLog NEWS README TODO \
	xmlxdump.cc

include build/node-end.mk

