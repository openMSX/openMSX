#ifndef DEVICECONFIG_HH
#define DEVICECONFIG_HH

#include <cassert>
#include <string_view>

namespace openmsx {

class XMLElement;
class XMLDocument;
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

	[[nodiscard]] const HardwareConfig& getHardwareConfig() const
	{
		assert(hwConf);
		return *hwConf;
	}
	[[nodiscard]] const XMLElement* getXML() const
	{
		return devConf;
	}
	[[nodiscard]] XMLElement* getPrimary() const
	{
		return const_cast<XMLElement*>(primary);
	}
	[[nodiscard]] XMLElement* getSecondary() const
	{
		return const_cast<XMLElement*>(secondary);
	}

	// convenience methods:
	//  methods below simply delegate to HardwareConfig or XMLElement
	[[nodiscard]] const FileContext& getFileContext() const;
	[[nodiscard]] MSXMotherBoard& getMotherBoard() const;
	[[nodiscard]] CliComm& getCliComm() const;
	[[nodiscard]] CommandController& getCommandController() const;
	[[nodiscard]] Scheduler& getScheduler() const;
	[[nodiscard]] Reactor& getReactor() const;
	[[nodiscard]] GlobalSettings& getGlobalSettings() const;
	[[nodiscard]] XMLDocument& getXMLDocument();

	[[nodiscard]] const XMLElement& getChild(std::string_view name) const;
	[[nodiscard]] std::string_view getChildData(std::string_view name) const;
	[[nodiscard]] std::string_view getChildData(std::string_view name,
	                                            std::string_view defaultValue) const;
	[[nodiscard]] int getChildDataAsInt(std::string_view name, int defaultValue) const;
	[[nodiscard]] bool getChildDataAsBool(std::string_view name,
	                                      bool defaultValue = false) const;
	[[nodiscard]] const XMLElement* findChild(std::string_view name) const;
	[[nodiscard]] std::string_view getAttributeValue(std::string_view attName) const;
	[[nodiscard]] int getAttributeValueAsInt(std::string_view attName, int defaultValue) const;

private:
	const HardwareConfig* hwConf = nullptr;
	const XMLElement* devConf = nullptr;
	const XMLElement* primary = nullptr;
	const XMLElement* secondary = nullptr;
};

} // namespace openmsx

#endif
