// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>
#include <iostream>
#include <sstream>

// sample xml msxconfig file is available in msxconfig.xml in this dir

#include "msxexception.hh"

// predefines
class XMLTree;
class XMLNode;

class MSXConfig
{
public:
	// this class is a singleton class
	// usage: MSXConfig::instance()->method(args);

	static MSXConfig *instance();

	void loadFile(const string &filename);
	//void saveFile(const string &filename = 0);

	class Device
	{
	public:
		class Parameter
		{
		public:
			Parameter(const string &name, const string &value, const string &clasz);
			~Parameter() {}
		private:
			Parameter(); // block usage
			Parameter(const Parameter &); // block usage
			Parameter &operator=(const Parameter &); // block usage
		public:
			const string name;
			const string value;
			const string clasz;
		};
		class Slotted
		{
		public:
			Slotted(int PS, int SS=-1, int Page=-1);
			~Slotted() {}
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

		Device(XMLNode *deviceNodeP);
		~Device();
		const string &getType();
		const string &getId();
		bool  isSlotted();
		int   getPage(); // of first slotted [backward compat]
		int   getPS(); // of first slotted [backward compat]
		int   getSS(); // of first slotted [backward compat]
		bool  hasParameter(const string &name);
		const string &getParameter(const string &name);
		list<const Parameter*> getParametersWithClass(const string &clasz);
		void  dump();
		const string &getDesc();
		const string &getRem();
	private:
		Device(); // block usage
		Device(const Device &foo); // block usage
		Device &operator=(const Device &foo); // block usage
		XMLNode *deviceNode;
	public:
		list <Slotted*> slotted;
	private:
		string id, deviceType, desc, rem;
		list<Parameter*> parameters;
	};

public:
	class Exception: public MSXException
	{
	public:
		Exception(const string &descs=""):MSXException(descs,0) {}
		Exception(const ostringstream &stream):MSXException(stream.str(),0) {}
	};

	class XMLParseException: public Exception
	{
	public:
		XMLParseException(const string &descs=""):Exception(descs) {}
		XMLParseException(const ostringstream &stream):Exception(stream) {}
	};

	list<Device*> deviceList;
	list<Device*> getDeviceByType(const string &type);
	Device *getDeviceById(const string &type);

private:
	MSXConfig(); // private constructor -> can only construct self
	~MSXConfig();
	MSXConfig(const MSXConfig &foo); // block usage
	MSXConfig &operator=(const MSXConfig &foo); // block usage

	static MSXConfig *volatile oneInstance;
	XMLTree *tree;
};

#endif
