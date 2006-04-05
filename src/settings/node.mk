# $Id$

include build/node-start.mk

SRC_HDR:= \
	SettingsManager \
	Setting \
	IntegerSetting \
	FloatSetting \
	StringSetting \
	BooleanSetting \
	FilenameSetting \
	KeyCodeSetting

HDR_ONLY:= \
	SettingImpl \
	EnumSetting \
	SettingPolicy \
	SettingRangePolicy

include build/node-end.mk

