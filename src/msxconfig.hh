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

// nested classes
#include "msxconfig_device.nn"
#include "msxconfig_exception.nn"

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
