// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>

// sample xml msxconfig file is available in msxconfig.xml in this dir

// predefines
class XMLTree;
class XMLNode;

class MSXConfig
{
public:
	// this class is a singleton class
	// usage: MSXConfig::instance()->method(args);

	static MSXConfig *instance();

	bool loadFile(const string &filename);
	//void saveFile(const string &filename = 0);

	class Device
	{
	public:
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
	private:
		Device(); // block usage
		Device(const Device &foo); // block usage
		Device &operator=(const Device &foo); // block usage
		XMLNode *deviceNode;
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
