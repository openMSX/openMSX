# $Id$

include build/node-start.mk

SRC_HDR:= \
	XMLLoader XMLElement \
	HardwareConfig SettingsConfig

HDR_ONLY:= \
	ConfigException

include build/node-end.mk

