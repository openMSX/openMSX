# $Id$

include build/node-start.mk

SUBDIRS:= \
	src build doc Contrib

DIST:= \
	alternative.mk \
	ChangeLog AUTHORS GPL README TODO \
	share

include build/node-end.mk

