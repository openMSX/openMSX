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
	FilenameSetting \
	KeyCodeSetting \
	UserSettings

HDR_ONLY:= \
	EnumSetting \
	SettingPolicy \
	SettingRangePolicy \
	ReadOnlySetting

include build/node-end.mk

