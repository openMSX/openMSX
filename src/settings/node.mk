# $Id$

include build/node-start.mk

SRC_HDR:= \
	SettingNode SettingsManager \
	IntegerSetting FloatSetting \
	BooleanSetting \
	StringSetting FilenameSetting

HDR_ONLY:= \
	Setting SettingListener \
	EnumSetting

include build/node-end.mk

