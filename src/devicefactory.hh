// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

#include "MSXDevice.hh"


class deviceFactory
{
	public:
		static MSXDevice *create(MSXConfig::Device *conf);
};
#endif
