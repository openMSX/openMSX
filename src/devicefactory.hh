// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

#include <string>
#include "MSXDevice.hh"


class deviceFactory
{
public:
	//static MSXDevice *create(const string &type);
	static MSXDevice *create(MSXConfig::Device *conf);
};
#endif
