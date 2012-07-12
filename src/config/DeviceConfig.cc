// $Id$

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

const XMLElement& DeviceConfig::getChild(string_ref name) const
{
	return getXML()->getChild(name);
}
const std::string& DeviceConfig::getChildData(string_ref name) const
{
	return getXML()->getChildData(name);
}
string_ref DeviceConfig::getChildData(string_ref name,
                                      string_ref defaultValue) const
{
	return getXML()->getChildData(name, defaultValue);
}
int DeviceConfig::getChildDataAsInt(string_ref name, int defaultValue) const
{
	return getXML()->getChildDataAsInt(name, defaultValue);
}
bool DeviceConfig::getChildDataAsBool(string_ref name,
                                      bool defaultValue) const
{
	return getXML()->getChildDataAsBool(name, defaultValue);
}
const XMLElement* DeviceConfig::findChild(string_ref name) const
{
	return getXML()->findChild(name);
}
const std::string& DeviceConfig::getAttribute(string_ref attName) const
{
	return getXML()->getAttribute(attName);
}
int DeviceConfig::getAttributeAsInt(string_ref attName, int defaultValue) const
{
	return getXML()->getAttributeAsInt(attName, defaultValue);
}

} // namespace openmsx
