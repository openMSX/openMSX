// $Id$

#include "msxconfig.hh"

// you need libxml++ 0.13 and libxml2 > 2.0.0
#include <xml++.h>

MSXConfig *volatile MSXConfig::oneInstance;

MSXConfig *MSXConfig::instance()
{
	if (oneInstance == 0)
		oneInstance = new MSXConfig;
	return oneInstance;
}

MSXConfig::MSXConfig():tree(0)
{
}

MSXConfig::~MSXConfig()
{
	if (tree != 0)
		delete tree;
}

bool MSXConfig::loadFile(const string &filename)
{
	tree = new XMLTree();
	if (!tree->read(filename))
		return false;
	cout << tree->write_buffer() << endl;
}
