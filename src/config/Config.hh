// $Id$

#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#include <string>
#include <vector>
#include "ConfigException.hh"

using std::string;
using std::vector;

namespace openmsx {

class FileContext;
class XMLElement;

class Config
{
public:
	static bool stringToBool(const string& str);
	static int stringToInt(const string& str);

	class Parameter {
	public:
		Parameter(const string& name,
			  const string& value,
			  const string& clasz);

		const string& getName() const { return name; };
		const string& getValue() const { return value; }
		const string& getClass() const { return clasz; }

		bool getAsBool() const;
		int getAsInt() const ;

	private:
		string name;
		string value;
		string clasz;
	};

	Config(XMLElement* element, FileContext& context);
	Config(const string& type, const string& id);
	virtual ~Config();

	const string& getType() const;
	const string& getId() const;

	FileContext& getContext() const;

	bool hasParameter(const string& name) const;
	const string &getParameter(const string& name) const throw(ConfigException);
	const string getParameter(const string& name, const string& defaultValue) const;
	bool getParameterAsBool(const string& name) const throw(ConfigException);
	bool getParameterAsBool(const string& name, bool defaultValue) const;
	int getParameterAsInt(const string& name) const throw(ConfigException);
	int getParameterAsInt(const string& name, int defaultValue) const;

	typedef vector<Parameter> Parameters;
	void getParametersWithClass(const string& clasz, Parameters& result);

protected:
	XMLElement* element;

private:
	XMLElement* getParameterElement(const string& name) const;

	FileContext* context;
	string type;
	string id;
};

} // namespace openmsx

#endif // __CONFIG_HH__
