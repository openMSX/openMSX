// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <vector>
#include <sstream>
#include "ConfigException.hh"
#include "FileException.hh"

using std::string;
using std::vector;
using std::ostringstream;

namespace openmsx {

class XMLDocument;
class Config;
class Device;
class FileContext;

class MSXConfig
{
public:
	static MSXConfig& instance();

	/**
	 * load a config file's content, and add it to
	 *  the config data [can be called multiple times]
	 */
	void loadHardware(FileContext& context, const string &filename)
		throw(FileException, ConfigException);
	void loadSetting(FileContext& context, const string &filename)
		throw(FileException, ConfigException);
	void loadStream(FileContext& context, const ostringstream &stream)
		throw(ConfigException);

	/**
	 * get a config or device or customconfig by id
	 */
	Config* getConfigById(const string& id) throw(ConfigException);
	Config* findConfigById(const string& id);
	bool hasConfigWithId(const string& id);
	Device* getDeviceById(const string& id) throw(ConfigException);

	typedef vector<Device*> Devices;
	const Devices& getDevices() const { return devices; }

private:
	MSXConfig();
	~MSXConfig();

	void handleDoc(const XMLDocument& doc, FileContext& context)
		throw(ConfigException);

	typedef vector<Config*> Configs;
	Configs configs;
	Devices devices;

	// Let GCC-3.2.3 be quiet...
	friend class dontGenerateWarningOnOlderCompilers;
};

} // namespace openmsx

#endif
