// $Id$

#ifndef __FDCFACTORY_HH__
#define __FDCFACTORY_HH__

class EmuTime;
class MSXDevice;
class Config;


class FDCFactory
{
	public:
		static MSXDevice* create(Device *config, const EmuTime &time);
};
#endif
