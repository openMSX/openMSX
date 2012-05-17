// $Id$

#ifndef DEVICECONFIG_HH
#define DEVICECONFIG_HH

#include "XMLElement.hh"
#include "HardwareConfig.hh"
#include <string>
#include <cassert>

namespace openmsx {

class HardwareConfig;
class FileContext;

class DeviceConfig
{
public:
	DeviceConfig()
		: hwConf(NULL), devConf(NULL)
	{
	}
	explicit DeviceConfig(const XMLElement& devConf_)
		: hwConf(NULL), devConf(&devConf_)
	{
	}
	DeviceConfig(const HardwareConfig& hwConf_, const XMLElement& devConf_)
		: hwConf(&hwConf_), devConf(&devConf_)
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

	// convenience methods:
	//  methods below simply delegate to HardwareConfig or XMLElement
	const FileContext& getFileContext() const
	{
		return getHardwareConfig().getFileContext();
	}
	const XMLElement& getChild(const char* name) const
	{
		return getXML()->getChild(name);
	}
	const std::string& getChildData(const char* name) const
	{
		return getXML()->getChildData(name);
	}
	std::string getChildData(const char* name,
	                         const char* defaultValue) const
	{
		return getXML()->getChildData(name, defaultValue);
	}
	int getChildDataAsInt(const char* name, int defaultValue = 0) const
	{
		return getXML()->getChildDataAsInt(name, defaultValue);
	}
	bool getChildDataAsBool(const char* name,
	                        bool defaultValue = false) const
	{
		return getXML()->getChildDataAsBool(name, defaultValue);
	}
	const XMLElement* findChild(const char* name) const
	{
		return getXML()->findChild(name);
	}
	const std::string& getAttribute(const char* attName) const
	{
		return getXML()->getAttribute(attName);
	}
	int getAttributeAsInt(const char* attName, int defaultValue = 0) const
	{
		return getXML()->getAttributeAsInt(attName, defaultValue);
	}

private:
	const HardwareConfig* hwConf;
	const XMLElement* devConf;
};

} // namespace openmsx

#endif
