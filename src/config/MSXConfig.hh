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
	 * get a config or device or customconfig by id
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
