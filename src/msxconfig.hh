// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>

// sample xml msxconfig file is available in msxconfig.xml in this dir

// predefines
class XMLTree;
class XMLNode;

class MSXConfigDevice
{
public:
	MSXConfigDevice(XMLNode *deviceNode);
	const string &getType();
	const string &getId();
	bool  isSlotted();
	int   getPage();
	int   getPS();
	int   getSS();
	const string &getParameter(const string &name);
private:
	MSXConfigDevice() {};
};

class MSXConfig
{
public:
	// this class is a singleton class
	// usage: MSXConfig::instance()->method(args);

	static MSXConfig *instance();

	bool loadFile(const string &filename);
	//void saveFile(const string &filename = 0);

	list<MSXConfigDevice*> deviceList;
	list<MSXConfigDevice*> getDeviceByType(const string &type);
	MSXConfigDevice *getDeviceById(const string &type);

private:
	MSXConfig(); // private constructor -> can only construct self
	~MSXConfig();
	MSXConfig(const MSXConfig &foo) {} // block usage
	MSXConfig &operator=(const MSXConfig &foo) {} // block usage

	static MSXConfig *volatile oneInstance;
	XMLTree *tree;
};

#endif
