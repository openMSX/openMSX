// $Id$
// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

// for uint64
#include "EmuTime.hh"

// TODO XXX rework msxexception
#include "MSXException.hh"

#include <string>
#include <list>
#include <sstream>

namespace MSXConfig
{

class Exception: public MSXException
{
public:
	Exception(const std::string &descs):MSXException(descs) {}
	Exception(const std::ostringstream &stream):MSXException(stream.str()) {}
};

class Config
{
public:

	// a parameter is the same for all backends:
	class Parameter
	{
	public:
		Parameter(const std::string &name, const std::string &value, const std::string &clasz);
		~Parameter();
	private:
		Parameter(); // block usage
		Parameter(const Parameter &); // block usage
		Parameter &operator=(const Parameter &); // block usage

	public:
		const bool getAsBool() const;
		const int getAsInt() const ;
		const uint64 getAsUint64() const;
		// please let me know what types are needed exactly
		
		const std::string name;
		const std::string value;
		const std::string clasz;

		static bool stringToBool(const std::string &str);
		static int stringToInt(const std::string &str);
		static uint64 stringToUint64(const std::string &str);
	};

	Config();
	virtual ~Config();

	virtual bool isDevice();
	virtual const std::string &getType()=0;
	virtual const std::string &getId()=0;
	virtual const std::string &getDesc()=0;
	virtual const std::string &getRem()=0;

	virtual bool hasParameter(const std::string &name)=0;
	virtual const std::string &getParameter(const std::string &name)=0;
	virtual const bool getParameterAsBool(const std::string &name)=0;
	virtual const int getParameterAsInt(const std::string &name)=0;
	virtual const uint64 getParameterAsUint64(const std::string &name)=0;

	/**
	 * This returns a freshly allocated list with freshly allocated
	 * Parameter objects. The caller has to clean this all up with
	 * getParametersWithClassClean
	 */
	virtual std::list<Parameter*>* getParametersWithClass(const std::string &clasz)=0;
	/**
	 * cleanup function for getParametersWithClass
	 */
	static void getParametersWithClassClean(std::list<Parameter*>* list);

	virtual void dump();

};

class Device: virtual public Config
{
public:

	// a slotted is the same for all backends:
	class Slotted
	{
	public:
		Slotted(int PS, int SS=-1, int Page=-1);
		~Slotted();
	private:
		Slotted(); // block usage
		Slotted(const Slotted &); // block usage
		Slotted &operator=(const Slotted &); // block usage
		int ps;
		int ss;
		int page;
	public:
		bool hasSS();
		bool hasPage();
		int getPS();
		int getSS();
		int getPage();

		void dump();
	};

	Device();
	virtual ~Device();

	virtual bool isDevice();
	bool  isSlotted();
	int   getPage(); // of first slotted [backward compat]
	int   getPS(); // of first slotted [backward compat]
	int   getSS(); // of first slotted [backward compat]

	std::list <Slotted*> slotted;

	virtual void dump();
	
private:
	Device(const Device &foo); // block usage
	Device &operator=(const Device &foo); // block usage
};

class Backend
{
public:
	/**
	 * load a config file's content, and add it
	 * to the config data [can be called multiple
	 * times]
	 */
	virtual void loadFile(const std::string &filename)=0;
	virtual void loadStream(const std::ostringstream &stream)=0;

	/**
	 * save current config to file
	 */
	virtual void saveFile()=0;
	virtual void saveFile(const std::string &filename)=0;

	/**
	 * get a config or device by id
	 */
	virtual Config* getConfigById(const std::string &id)=0;
	virtual Device* getDeviceById(const std::string &id)=0;

	/**
	 * get a device
	 */
	virtual void initDeviceIterator()=0;
	virtual Device* getNextDevice()=0;

	/**
	 * backend factory
	 * create a backend, by name
	 * currently only one backend: "xml"
	 */
	static Backend* createBackend(const std::string &name);

	/**
	 * returns the one backend, for backwards compat
	 */
	static Backend* instance();

	virtual ~Backend();

protected:
	Backend();
	// only I create instances of me

private:
	static Backend* _instance;

};

}; // end namespace MSXConfig

#endif

#ifndef __XMLCONFIG_HH__
#define __XMLCONFIG_HH__

#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"

namespace XMLConfig
{

class Config: virtual public MSXConfig::Config
{
public:
	Config(XML::Element *element);
	virtual ~Config();

	virtual const std::string &getType();
	virtual const std::string &getId();
	virtual const std::string &getDesc();
	virtual const std::string &getRem();

	virtual bool hasParameter(const std::string &name);
	virtual const std::string &getParameter(const std::string &name);
	virtual const bool getParameterAsBool(const std::string &name);
	virtual const int getParameterAsInt(const std::string &name);
	virtual const uint64 getParameterAsUint64(const std::string &name);
	virtual std::list<Parameter*>* getParametersWithClass(const std::string &clasz);

	virtual void dump();

private:
	Config(); // block usage
	Config(const Config &foo);            // block usage
	Config &operator=(const Config &foo); // block usage

	XML::Element* element;

	XML::Element* getParameterElement(const std::string &name);
};

class Device: public XMLConfig::Config, public MSXConfig::Device
{
public:
	Device(XML::Element *element);
	virtual ~Device();

	virtual void dump();

private:
	Device(); // block usage
	Device(const Device &foo);            // block usage
	Device &operator=(const Device &foo); // block usage
};

class Backend: public MSXConfig::Backend
{

// my factory is my friend:
friend MSXConfig::Backend* MSXConfig::Backend::createBackend(const std::string &name);

public:
	virtual void loadFile(const std::string &filename);
	virtual void loadStream(const std::ostringstream &stream);
	virtual void saveFile();
	virtual void saveFile(const std::string &filename);

	virtual MSXConfig::Config* getConfigById(const std::string &id);
	virtual MSXConfig::Device* getDeviceById(const std::string &id);

	virtual void initDeviceIterator();
	virtual MSXConfig::Device* getNextDevice();

protected:
	virtual ~Backend();
	Backend();

private:
	Backend(const Backend &foo);            // block usage
	Backend &operator=(const Backend &foo); // block usage

	bool hasConfigWithId(const std::string &id);

	std::list<XML::Document*> docs;
	std::list<Config*> configs;
	std::list<Device*> devices;

	std::list<Device*>::const_iterator device_iterator;

	void handleDoc(XML::Document* doc);
};

}; // end namespace XMLConfig

#endif // defined __XMLCONFIG_HH__
