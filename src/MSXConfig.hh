// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

// for uint64
#include "EmuTime.hh"

// TODO XXX rework msxexception
#include "msxexception.hh"

#include <string>
#include <list>
#include <sstream>

namespace MSXConfig
{

class Exception: public MSXException
{
public:
	Exception(const std::string &descs=""):MSXException(descs,0) {}
	Exception(const std::ostringstream &stream):MSXException(stream.str(),0) {}
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
	};

	Config();
	virtual ~Config();

	virtual bool isDevice();
	virtual const std::string &getType()=0;
	virtual const std::string &getId()=0;
};

class Device: public Config
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
	};

	Device();
	virtual ~Device();

	virtual bool isDevice();
	bool  isSlotted();
	int   getPage(); // of first slotted [backward compat]
	int   getPS(); // of first slotted [backward compat]
	int   getSS(); // of first slotted [backward compat]

	std::list <Slotted*> slotted;
	
private:
	Device(const Device &foo); // block usage
	Device &operator=(const Device &foo); // block usage
};

class Backend
{
public:
	virtual void loadFile(const std::string &filename)=0;
	virtual void saveFile()=0;
	virtual void saveFile(const std::string &filename)=0;

	virtual Config* getConfigById(const std::string &type)=0;

	/**
	 * backend factory
	 * create a backend
	 */
	static Backend* createBackend(const std::string &name);

protected:
	virtual ~Backend();
	// only I create instances of me
	Backend();

};

}; // end namespace MSXConfig

#endif
