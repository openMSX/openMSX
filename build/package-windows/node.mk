include build/node-start.mk

SUBDIRS:= \
	images

DIST:= \
	controlpanel.wxi openmsx.wxs openmsx1033.wxl *.py \
	package.cmd

include build/node-end.mk
