# $Id$

include build/node-start.mk

SRC_HDR:= \
	SettingsManager \
	Setting \
	SettingImpl \
	ProxySetting \
	IntegerSetting \
	FloatSetting \
	StringSetting \
	BooleanSetting \
	EnumSetting \
	FilenameSetting \
	KeyCodeSetting \
	UserSettings

HDR_ONLY:= \
	SettingPolicy \
	SettingRangePolicy \
	ReadOnlySetting

include build/node-end.mk

