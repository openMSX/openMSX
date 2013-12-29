include build/node-start.mk

SUBDIRS:= \
	openmsx

DIST:= \
	setup_anddev.sh \
	*.mk \
	*.patch

include build/node-end.mk
