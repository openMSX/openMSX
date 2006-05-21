# $Id$

include build/node-start.mk

SRC_HDR:= \
	SunriseIDE \
	DummyIDEDevice \
	IDEDeviceFactory \
	AbstractIDEDevice IDEHD

HDR_ONLY:= \
	IDEDevice

include build/node-end.mk

