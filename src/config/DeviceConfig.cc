// $Id$

#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "HardwareConfig.hh"
#include "MSXMotherBoard.hh"

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

const XMLElement& DeviceConfig::getChild(const char* name) const
{
	return getXML()->getChild(name);
}
const std::string& DeviceConfig::getChildData(const char* name) const
{
	return getXML()->getChildData(name);
}
std::string DeviceConfig::getChildData(const char* name,
                                       const char* defaultValue) const
{
	return getXML()->getChildData(name, defaultValue);
}
int DeviceConfig::getChildDataAsInt(const char* name, int defaultValue) const
{
	return getXML()->getChildDataAsInt(name, defaultValue);
}
bool DeviceConfig::getChildDataAsBool(const char* name,
                                      bool defaultValue) const
{
	return getXML()->getChildDataAsBool(name, defaultValue);
}
const XMLElement* DeviceConfig::findChild(const char* name) const
{
	return getXML()->findChild(name);
}
const std::string& DeviceConfig::getAttribute(const char* attName) const
{
	return getXML()->getAttribute(attName);
}
int DeviceConfig::getAttributeAsInt(const char* attName, int defaultValue) const
{
	return getXML()->getAttributeAsInt(attName, defaultValue);
}

} // namespace openmsx
