include build/node-start.mk

SRC_HDR:= \
	SettingsManager \
	Setting \
	ProxySetting \
	IntegerSetting \
	FloatSetting \
	StringSetting \
	BooleanSetting \
	EnumSetting \
	FilenameSetting \
	KeyCodeSetting \
	VideoSourceSetting \
	ReadOnlySetting \
	UserSettings

include build/node-end.mk
