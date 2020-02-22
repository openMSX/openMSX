#ifndef DEVICECONFIG_HH
#define DEVICECONFIG_HH

#include <cassert>
#include <string_view>

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
	DeviceConfig() = default;
	DeviceConfig(const HardwareConfig& hwConf_, const XMLElement& devConf_)
		: hwConf(&hwConf_), devConf(&devConf_)
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
	{
	}
	DeviceConfig(const DeviceConfig& other, const XMLElement* devConf_)
		: hwConf(other.hwConf), devConf(devConf_)
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

	const XMLElement& getChild(std::string_view name) const;
	const std::string& getChildData(std::string_view name) const;
	std::string_view getChildData(std::string_view name,
	                              std::string_view defaultValue) const;
	int getChildDataAsInt(std::string_view name, int defaultValue = 0) const;
	bool getChildDataAsBool(std::string_view name,
	                        bool defaultValue = false) const;
	const XMLElement* findChild(std::string_view name) const;
	const std::string& getAttribute(std::string_view attName) const;
	int getAttributeAsInt(std::string_view attName, int defaultValue = 0) const;

private:
	const HardwareConfig* hwConf = nullptr;
	const XMLElement* devConf = nullptr;
	const XMLElement* primary = nullptr;
	const XMLElement* secondary = nullptr;
};

} // namespace openmsx

#endif
