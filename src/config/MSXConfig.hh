// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <vector>
#include "ConfigException.hh"

using std::string;
using std::vector;

namespace openmsx {

class XMLDocument;
class XMLElement;
class Config;
class FileContext;

class MSXConfig
{
public:
	void loadConfig(const XMLElement& config, const FileContext& context)
		throw(ConfigException);

	Config* getConfigById(const string& id) throw(ConfigException);
	Config* findConfigById(const string& id);
	bool hasConfigWithId(const string& id);

	typedef vector<Config*> Configs;
	const Configs& getConfigs() const { return configs; }

protected:
	MSXConfig();
	~MSXConfig();

	void handleDoc(const XMLDocument& doc, FileContext& context)
		throw(ConfigException);

	Configs configs;
};

} // namespace openmsx

#endif
