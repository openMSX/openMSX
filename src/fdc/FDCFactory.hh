// $Id$

#ifndef __FDCFACTORY_HH__
#define __FDCFACTORY_HH__

#include "MSXConfig.hh"

class EmuTime;
class MSXDevice;


class FDCFactory
{
	public:
		static MSXDevice* create(Device *config, const EmuTime &time);
};
#endif
