# $Id$

SRC_HDR:= \
	SettingNode SettingsManager \
	IntegerSetting FloatSetting \
	EnumSetting BooleanSetting \
	StringSetting FilenameSetting

HDR_ONLY:= \
	Setting SettingListener

$(eval $(PROCESS_NODE))

