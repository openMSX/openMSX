// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

// forward declaration
class MSXDevice;
class EmuTime;


class deviceFactory
{
	public:
		static MSXDevice *create(MSXConfig::Device *conf, const EmuTime &time);
};
#endif
