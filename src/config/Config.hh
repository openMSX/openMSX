// $Id$

#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#include <string>
#include <list>
#include "openmsx.hh"
#include "libxmlx/xmlx.hh"
#include "ConfigException.hh"

using std::string;
using std::list;

namespace openmsx {

class FileContext;

class Config
{
public:
	// a parameter is the same for all backends:
	class Parameter {
	public:
		Parameter(const string& name, 
			  const string& value,
			  const string& clasz);
		~Parameter();

		const bool getAsBool() const;
		const int getAsInt() const ;
		const uint64 getAsUint64() const;
		
		const string name;
		const string value;
		const string clasz;

		static bool stringToBool(const string& str);
		static int stringToInt(const string& str);
		static uint64 stringToUint64(const string& str);
	};

	Config(XML::Element* element, FileContext& context);
	Config(const string& type, const string& id);
	virtual ~Config();

	const string& getType() const;
	const string& getId() const;

	FileContext& getContext() const;

	bool hasParameter(const string& name) const;
	const string &getParameter(const string& name) const throw(ConfigException);
	const string getParameter(const string& name, const string& defaultValue) const;
	const bool getParameterAsBool(const string& name) const throw(ConfigException);
	const bool getParameterAsBool(const string& name, bool defaultValue) const;
	const int getParameterAsInt(const string& name) const throw(ConfigException);
	const int getParameterAsInt(const string& name, int defaultValue) const;
	const uint64 getParameterAsUint64(const string& name) const throw(ConfigException);
	const uint64 getParameterAsUint64(const string& name, const uint64& defaultValue) const;

	/**
	 * This returns a freshly allocated list with freshly allocated
	 * Parameter objects. The caller has to clean this all up with
	 * getParametersWithClassClean
	 */
	virtual list<Parameter*>* getParametersWithClass(const string& clasz);
	/**
	 * cleanup function for getParametersWithClass
	 */
	static void getParametersWithClassClean(list<Parameter*>* list);

protected:
	XML::Element* element;
	FileContext* context;
	string type;
	string id;

private:
	XML::Element* getParameterElement(const string& name) const;
};

} // namespace openmsx

#endif
