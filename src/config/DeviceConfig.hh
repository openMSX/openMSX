// $Id$

#ifndef DEVICECONFIG_HH
#define DEVICECONFIG_HH

#include <string>
#include <cassert>

namespace openmsx {

class XMLElement;
class HardwareConfig;
class FileContext;
class MSXMotherBoard;
class CliComm;
class CommandController;
class Scheduler;

class DeviceConfig
{
public:
	DeviceConfig()
		: hwConf(NULL), devConf(NULL)
		, primary(NULL), secondary(NULL)
	{
	}
	DeviceConfig(const HardwareConfig& hwConf_, const XMLElement& devConf_)
		: hwConf(&hwConf_), devConf(&devConf_)
		, primary(NULL), secondary(NULL)
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
		, primary(NULL), secondary(NULL)
	{
	}
	DeviceConfig(const DeviceConfig& other, const XMLElement* devConf_)
		: hwConf(other.hwConf), devConf(devConf_)
		, primary(NULL), secondary(NULL)
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

	const XMLElement& getChild(const char* name) const;
	const std::string& getChildData(const char* name) const;
	std::string getChildData(const char* name,
	                         const char* defaultValue) const;
	int getChildDataAsInt(const char* name, int defaultValue = 0) const;
	bool getChildDataAsBool(const char* name,
	                        bool defaultValue = false) const;
	const XMLElement* findChild(const char* name) const;
	const std::string& getAttribute(const char* attName) const;
	int getAttributeAsInt(const char* attName, int defaultValue = 0) const;

private:
	const HardwareConfig* hwConf;
	const XMLElement* devConf;
	const XMLElement* primary;
	const XMLElement* secondary;
};

} // namespace openmsx

#endif
