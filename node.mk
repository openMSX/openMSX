# $Id$

include build/node-start.mk

SUBDIRS:= \
	src build doc Contrib

DIST:= \
	GNUmakefile configure \
	ChangeLog AUTHORS GPL README TODO \
	share

include build/node-end.mk

