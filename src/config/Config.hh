// $Id$

#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#include "xmlx.hh"

namespace openmsx {

class FileContext;

class Config : public XMLElement
{
public:
	Config(const XMLElement& element, const FileContext& context);
	~Config();

	const string& getType() const;
	const string& getId() const;

	FileContext& getContext() const;

	bool hasParameter(const string& name) const;
	const string& getParameter(const string& name) const;
	const string getParameter(const string& name, const string& defaultValue) const;
	bool getParameterAsBool(const string& name) const;
	bool getParameterAsBool(const string& name, bool defaultValue) const;
	int getParameterAsInt(const string& name) const;
	int getParameterAsInt(const string& name, int defaultValue) const;

private:
	FileContext* context;
};

} // namespace openmsx

#endif // __CONFIG_HH__
