# $Id$

SRC_HDR:= \
	SettingNode SettingsManager \
	IntegerSetting FloatSetting \
	BooleanSetting \
	StringSetting FilenameSetting

HDR_ONLY:= \
	Setting SettingListener \
	EnumSetting

$(eval $(PROCESS_NODE))

