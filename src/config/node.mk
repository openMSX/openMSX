# $Id$

include build/node-start.mk

SRC_HDR:= \
	XMLLoader XMLElement \
	HardwareConfig \
	SettingsConfig \
	DeviceConfig \

HDR_ONLY:= \
	ConfigException \
	XMLException \

include build/node-end.mk

