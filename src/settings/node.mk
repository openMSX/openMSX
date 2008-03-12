# $Id$

include build/node-start.mk

SRC_HDR:= \
	SettingsManager \
	Setting \
	ProxySetting \
	IntegerSetting \
	FloatSetting \
	StringSetting \
	BooleanSetting \
	FilenameSetting \
	KeyCodeSetting \
	UserSettings

HDR_ONLY:= \
	SettingImpl \
	EnumSetting \
	SettingPolicy \
	SettingRangePolicy \
	ReadOnlySetting

include build/node-end.mk

