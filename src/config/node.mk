# $Id$

include build/node-start.mk

SRC_HDR:= \
	MSXConfig Config xmlx \
	HardwareConfig SettingsConfig

HDR_ONLY:= \
	ConfigException

include build/node-end.mk

