// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>

// sample xml msxconfig file is available in msxconfig.xml in this dir

// predefines
class XMLTree;

class MSXConfig
{
public:
	// this class is a singleton class
	// usage: MSXConfig::instance()->method(args);

	static MSXConfig *instance();

	int loadFile(const string &filename);
	//void saveFile(const string &filename = 0);

private:
	MSXConfig(); // private constructor -> can only construct self
	~MSXConfig();
	MSXConfig(const MSXConfig &foo) {} // block usage
	MSXConfig &operator=(const MSXConfig &foo) {} // block usage

	static MSXConfig *volatile oneInstance;
	XMLTree *tree;
};

#endif
