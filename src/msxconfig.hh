// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>
#include <hash_map>

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
		Device(XMLNode *deviceNodeP);
		~Device();
		const string &getType();
		const string &getId();
		bool  isSlotted();
		int   getPage();
		int   getPS();
		int   getSS();
		bool  hasParameter(const string &name);
		const string &getParameter(const string &name);
		list<const Parameter*> getParametersWithClass(const string &clasz);
		void  dump();
	private:
		Device(); // block usage
		Device(const Device &foo); // block usage
		Device &operator=(const Device &foo); // block usage
		XMLNode *deviceNode;
		string id, deviceType;
		int page, ps, ss;
		bool slotted;
		list<Parameter*> parameters;
	};

public:
	class Exception: public MSXException
	{
	public:
		Exception(const string &descs=""):MSXException(descs,0) {}
	};

	class XMLParseException: public Exception
	{
	public:
		XMLParseException(const string &descs=""):Exception(descs) {}
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
