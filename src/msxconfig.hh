// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>
#include <iostream>
#include <sstream>

// sample xml msxconfig file is available in msxconfig.xml in this dir

#include "msxexception.hh"
#include "EmuTime.hh"

// predefines
class XMLTree;
class XMLNode;

class MSXConfig
{
public:
	// this class is a singleton class
	// usage: MSXConfig::instance()->method(args);

	static MSXConfig *instance();

	void loadFile(const std::string &filename);
	void saveFile();
	void saveFile(const std::string &filename);

// nested classes
#include "msxconfig_config.nn"
#include "msxconfig_device.nn"
#include "msxconfig_exception.nn"

	std::list<Config*> configList;
	std::list<Device*> deviceList;
	std::list<Config*> allList;
	//std::list<Config*> getConfigByType(const std::string &type);
	Config *getConfigById(const std::string &type);

private:
	MSXConfig(); // private constructor -> can only construct self
	~MSXConfig();
	MSXConfig(const MSXConfig &foo); // block usage
	MSXConfig &operator=(const MSXConfig &foo); // block usage

	static MSXConfig *volatile oneInstance;
	XMLTree *tree;
	std::string loaded_filename;
};

#endif
