# $Id$

include build/node-start.mk

SRC_HDR:= \
	SettingsManager \
	Setting \
	IntegerSetting \
	FloatSetting \
	StringSetting \
	BooleanSetting \
	FilenameSetting

HDR_ONLY:= \
	SettingImpl \
	SettingListener \
	EnumSetting

include build/node-end.mk

