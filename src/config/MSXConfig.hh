// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>
#include "openmsx.hh"
#include "MSXException.hh"
#include "libxmlx/xmlx.hh"
#include "FileContext.hh"
#include "File.hh"

using std::string;
using std::list;

namespace openmsx {

class ConfigException: public MSXException
{
public:
	ConfigException(const string &descs)
		: MSXException(descs) {}
};

class Config
{
public:
	// a parameter is the same for all backends:
	class Parameter
	{
	public:
		Parameter(const string &name, 
			  const string &value,
			  const string &clasz);
		~Parameter();

		const bool getAsBool() const;
		const int getAsInt() const ;
		const uint64 getAsUint64() const;
		
		const string name;
		const string value;
		const string clasz;

		static bool stringToBool(const string &str);
		static int stringToInt(const string &str);
		static uint64 stringToUint64(const string &str);
	};

	Config(XML::Element *element, FileContext& context);
	virtual ~Config();

	const string &getType() const;
	const string &getId() const;

	FileContext& getContext() const;

	bool hasParameter(const string &name) const;
	const string &getParameter(const string &name) const throw(ConfigException);
	const string getParameter(const string &name, const string &defaultValue) const;
	const bool getParameterAsBool(const string &name) const throw(ConfigException);
	const bool getParameterAsBool(const string &name, bool defaultValue) const;
	const int getParameterAsInt(const string &name) const throw(ConfigException);
	const int getParameterAsInt(const string &name, int defaultValue) const;
	const uint64 getParameterAsUint64(const string &name) const throw(ConfigException);
	const uint64 getParameterAsUint64(const string &name, const uint64 &defaultValue) const;

	/**
	 * This returns a freshly allocated list with freshly allocated
	 * Parameter objects. The caller has to clean this all up with
	 * getParametersWithClassClean
	 */
	virtual list<Parameter*>* getParametersWithClass(const string &clasz);
	/**
	 * cleanup function for getParametersWithClass
	 */
	static void getParametersWithClassClean(list<Parameter*>* list);

protected:
	XML::Element* element;

private:
	XML::Element* getParameterElement(const string &name) const;
	
	FileContext* context;
};

class Device: virtual public Config
{
public:
	// a slotted is the same for all backends:
	class Slotted
	{
	public:
		Slotted(int PS, int SS, int Page);
		~Slotted();
		
		int getPS() const;
		int getSS() const;
		int getPage() const;

	private:
		int ps;
		int ss;
		int page;
	};

	Device(XML::Element *element, FileContext &context);
	virtual ~Device();

	list <Slotted*> slotted;
};


class MSXConfig
{
public:
	/**
	 * load a config file's content, and add it to
	 *  the config data [can be called multiple times]
	 */
	void loadHardware(FileContext &context, const string &filename)
		throw(FileException, ConfigException);
	void loadSetting(FileContext &context, const string &filename)
		throw(FileException, ConfigException);
	void loadStream(FileContext &context, const ostringstream &stream)
		throw(ConfigException);

	/**
	 * save current config to file
	 */
	void saveFile();
	void saveFile(const string &filename);

	/**
	 * get a config or device or customconfig by id
	 */
	Config* getConfigById(const string &id) throw(ConfigException);
	Config* findConfigById(const string &id);
	bool hasConfigWithId(const string &id);
	Device* getDeviceById(const string &id) throw(ConfigException);

	/**
	 * get a device
	 */
	void initDeviceIterator();
	Device* getNextDevice();

	/**
	 * returns the one backend, for backwards compat
	 */
	static MSXConfig* instance();

	~MSXConfig();

private:
	MSXConfig();

	void handleDoc(XML::Document* doc, FileContext &context) throw(ConfigException);

	list<XML::Document*> docs;
	list<Config*> configs;
	list<Device*> devices;

	list<Device*>::const_iterator device_iterator;
};

} // namespace openmsx

#endif
