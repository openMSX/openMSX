// $Id$

#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#include <string>
#include <map>
#include "ConfigException.hh"
#include "xmlx.hh"

using std::string;
using std::multimap;

namespace openmsx {

class FileContext;
class XMLElement;

class Config
{
public:
	Config(const XMLElement& element, const FileContext& context);
	virtual ~Config();

	const string& getType() const;
	const string& getId() const;

	const XMLElement& getXMLElement() const;
	FileContext& getContext() const;

	bool hasParameter(const string& name) const;
	const string &getParameter(const string& name) const;
	const string getParameter(const string& name, const string& defaultValue) const;
	bool getParameterAsBool(const string& name) const;
	bool getParameterAsBool(const string& name, bool defaultValue) const;
	int getParameterAsInt(const string& name) const;
	int getParameterAsInt(const string& name, int defaultValue) const;

	typedef multimap<string, string> Parameters;
	void getParametersWithClass(const string& clasz, Parameters& result);

private:
	XMLElement* getParameterElement(const string& name) const;

	XMLElement element;
	FileContext* context;
};

} // namespace openmsx

#endif // __CONFIG_HH__
