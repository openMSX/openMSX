#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "HardwareConfig.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

namespace openmsx {

const FileContext& DeviceConfig::getFileContext() const
{
	return getHardwareConfig().getFileContext();
}
MSXMotherBoard& DeviceConfig::getMotherBoard() const
{
	return getHardwareConfig().getMotherBoard();
}

CliComm& DeviceConfig::getCliComm() const
{
	return getMotherBoard().getMSXCliComm();
}
CommandController& DeviceConfig::getCommandController() const
{
	return getMotherBoard().getCommandController();
}
Scheduler& DeviceConfig::getScheduler() const
{
	return getMotherBoard().getScheduler();
}
Reactor& DeviceConfig::getReactor() const
{
	return getMotherBoard().getReactor();
}
GlobalSettings& DeviceConfig::getGlobalSettings() const
{
	return getReactor().getGlobalSettings();
}

const XMLElement& DeviceConfig::getChild(std::string_view name) const
{
	return getXML()->getChild(name);
}
std::string_view DeviceConfig::getChildData(std::string_view name) const
{
	return getXML()->getChildData(name);
}
std::string_view DeviceConfig::getChildData(std::string_view name,
                                            std::string_view defaultValue) const
{
	return getXML()->getChildData(name, defaultValue);
}
int DeviceConfig::getChildDataAsInt(std::string_view name, int defaultValue) const
{
	return getXML()->getChildDataAsInt(name, defaultValue);
}
bool DeviceConfig::getChildDataAsBool(std::string_view name,
                                      bool defaultValue) const
{
	return getXML()->getChildDataAsBool(name, defaultValue);
}
const XMLElement* DeviceConfig::findChild(std::string_view name) const
{
	return getXML()->findChild(name);
}
std::string_view DeviceConfig::getAttributeValue(std::string_view attName) const
{
	return getXML()->getAttributeValue(attName);
}
int DeviceConfig::getAttributeValueAsInt(std::string_view attName, int defaultValue) const
{
	return getXML()->getAttributeValueAsInt(attName, defaultValue);
}

} // namespace openmsx
