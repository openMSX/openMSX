# $Id$

include build/node-start.mk

SRC_HDR:= \
	SunriseIDE \
	DummyIDEDevice \
	IDEDeviceFactory \
	AbstractIDEDevice IDEHD IDECDROM \
	HDImageCLI CDImageCLI \
	SCSIDevice WD33C93 GoudaSCSI

HDR_ONLY:= \
	IDEDevice \
	SCSI

include build/node-end.mk

