#ifndef DEVICECONFIG_HH
#define DEVICECONFIG_HH

#include "string_view.hh"
#include <cassert>

namespace openmsx {

class XMLElement;
class HardwareConfig;
class FileContext;
class MSXMotherBoard;
class CliComm;
class CommandController;
class Scheduler;
class Reactor;
class GlobalSettings;

class DeviceConfig
{
public:
	DeviceConfig()
		: hwConf(nullptr), devConf(nullptr)
		, primary(nullptr), secondary(nullptr)
	{
	}
	DeviceConfig(const HardwareConfig& hwConf_, const XMLElement& devConf_)
		: hwConf(&hwConf_), devConf(&devConf_)
		, primary(nullptr), secondary(nullptr)
	{
	}
	DeviceConfig(const HardwareConfig& hwConf_, const XMLElement& devConf_,
	             const XMLElement* primary_, const XMLElement* secondary_)
		: hwConf(&hwConf_), devConf(&devConf_)
		, primary(primary_), secondary(secondary_)
	{
	}
	DeviceConfig(const DeviceConfig& other, const XMLElement& devConf_)
		: hwConf(other.hwConf), devConf(&devConf_)
		, primary(nullptr), secondary(nullptr)
	{
	}
	DeviceConfig(const DeviceConfig& other, const XMLElement* devConf_)
		: hwConf(other.hwConf), devConf(devConf_)
		, primary(nullptr), secondary(nullptr)
	{
	}

	const HardwareConfig& getHardwareConfig() const
	{
		assert(hwConf);
		return *hwConf;
	}
	const XMLElement* getXML() const
	{
		return devConf;
	}
	XMLElement* getPrimary() const
	{
		return const_cast<XMLElement*>(primary);
	}
	XMLElement* getSecondary() const
	{
		return const_cast<XMLElement*>(secondary);
	}

	// convenience methods:
	//  methods below simply delegate to HardwareConfig or XMLElement
	const FileContext& getFileContext() const;
	MSXMotherBoard& getMotherBoard() const;
	CliComm& getCliComm() const;
	CommandController& getCommandController() const;
	Scheduler& getScheduler() const;
	Reactor& getReactor() const;
	GlobalSettings& getGlobalSettings() const;

	const XMLElement& getChild(string_view name) const;
	const std::string& getChildData(string_view name) const;
	string_view getChildData(string_view name,
	                        string_view defaultValue) const;
	int getChildDataAsInt(string_view name, int defaultValue = 0) const;
	bool getChildDataAsBool(string_view name,
	                        bool defaultValue = false) const;
	const XMLElement* findChild(string_view name) const;
	const std::string& getAttribute(string_view attName) const;
	int getAttributeAsInt(string_view attName, int defaultValue = 0) const;

private:
	const HardwareConfig* hwConf;
	const XMLElement* devConf;
	const XMLElement* primary;
	const XMLElement* secondary;
};

} // namespace openmsx

#endif
